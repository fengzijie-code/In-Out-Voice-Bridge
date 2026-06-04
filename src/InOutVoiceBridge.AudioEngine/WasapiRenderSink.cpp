#include "WasapiRenderSink.h"
#include "RingBuffer.h"
#include "AudioFormatConverter.h"

#include <functiondiscoverykeys_devpkey.h>
#include <avrt.h>
#include <cstring>
#include <vector>
#include <cmath>

#pragma comment(lib, "avrt.lib")

WasapiRenderSink::WasapiRenderSink() {
    m_renderEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

WasapiRenderSink::~WasapiRenderSink() {
    Stop();
    if (m_renderEvent) CloseHandle(m_renderEvent);
}

HRESULT WasapiRenderSink::Start(LPCWSTR deviceId, const WAVEFORMATEX* sourceFormat, RingBuffer* ringBuffer) {
    if (m_rendering.load()) return E_FAIL;
    m_ringBuffer = ringBuffer;
    m_sourceFormat = *sourceFormat;

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) return hr;

    hr = enumerator->GetDevice(deviceId, &m_device);
    enumerator->Release();
    if (FAILED(hr)) return hr;

    hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_audioClient);
    if (FAILED(hr)) return hr;

    WAVEFORMATEX* mixFormat = nullptr;
    hr = m_audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr)) return hr;

    m_renderFormat = *mixFormat;

    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        0, 0, mixFormat, nullptr);
    CoTaskMemFree(mixFormat);
    if (FAILED(hr)) return hr;

    hr = m_audioClient->SetEventHandle(m_renderEvent);
    if (FAILED(hr)) return hr;

    hr = m_audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_renderClient);
    if (FAILED(hr)) return hr;

    m_rendering.store(true);
    hr = m_audioClient->Start();
    if (FAILED(hr)) {
        m_rendering.store(false);
        return hr;
    }

    m_thread = std::thread(&WasapiRenderSink::RenderThread, this);
    return S_OK;
}

void WasapiRenderSink::Stop() {
    m_rendering.store(false);
    if (m_thread.joinable()) {
        SetEvent(m_renderEvent);
        m_thread.join();
    }
    if (m_audioClient) {
        m_audioClient->Stop();
        m_audioClient->Release();
        m_audioClient = nullptr;
    }
    if (m_renderClient) {
        m_renderClient->Release();
        m_renderClient = nullptr;
    }
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
    m_rmsLevel.store(0.0f);
}

void WasapiRenderSink::RenderThread() {
    DWORD taskIndex = 0;
    HANDLE avrtHandle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    UINT32 bufferFrameCount = 0;
    m_audioClient->GetBufferSize(&bufferFrameCount);

    size_t bytesPerFrame = m_renderFormat.nBlockAlign;
    bool formatsMatch = AudioFormatConverter::FormatsMatch(&m_sourceFormat, &m_renderFormat);

    while (m_rendering.load()) {
        DWORD waitResult = WaitForSingleObject(m_renderEvent, 100);
        if (!m_rendering.load()) break;
        if (waitResult != WAIT_OBJECT_0) continue;

        UINT32 numFramesPadding = 0;
        HRESULT hr = m_audioClient->GetCurrentPadding(&numFramesPadding);
        if (FAILED(hr)) continue;

        UINT32 numFramesAvailable = bufferFrameCount - numFramesPadding;
        if (numFramesAvailable == 0) continue;

        BYTE* renderBuffer = nullptr;
        hr = m_renderClient->GetBuffer(numFramesAvailable, &renderBuffer);
        if (FAILED(hr)) continue;

        size_t bytesNeeded = numFramesAvailable * bytesPerFrame;
        size_t bytesRead = 0;

        if (formatsMatch) {
            bytesRead = m_ringBuffer->Read(renderBuffer, bytesNeeded);
        } else {
            std::vector<uint8_t> srcBuffer(bytesNeeded);
            size_t srcRead = m_ringBuffer->Read(srcBuffer.data(), bytesNeeded);
            if (srcRead > 0) {
                size_t converted = 0;
                if (AudioFormatConverter::ConvertBuffer(
                        srcBuffer.data(), srcRead, &m_sourceFormat,
                        renderBuffer, bytesNeeded, &m_renderFormat, &converted)) {
                    bytesRead = converted;
                }
            }
        }

        if (bytesRead < bytesNeeded) {
            memset(renderBuffer + bytesRead, 0, bytesNeeded - bytesRead);
        }

        if (bytesRead > 0 && m_renderFormat.wBitsPerSample == 32) {
            size_t sampleCount = bytesRead / sizeof(float);
            float rms = AudioFormatConverter::CalculateRms(
                reinterpret_cast<const float*>(renderBuffer), sampleCount);
            m_rmsLevel.store(rms);
        } else if (bytesRead == 0) {
            m_rmsLevel.store(0.0f);
        }

        m_renderClient->ReleaseBuffer(numFramesAvailable, bytesRead == 0 ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
    }

    if (avrtHandle) AvRevertMmThreadCharacteristics(avrtHandle);
}
