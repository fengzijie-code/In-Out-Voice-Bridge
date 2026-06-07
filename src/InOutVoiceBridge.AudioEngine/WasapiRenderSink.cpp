#include "WasapiRenderSink.h"
#include "RingBuffer.h"
#include "AudioFormatConverter.h"

#include <functiondiscoverykeys_devpkey.h>
#include <avrt.h>
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "avrt.lib")

namespace {
constexpr float MinGainDb = -60.0f;
constexpr float MaxGainDb = 20.0f;

void ApplyGainFloat32(BYTE* buffer, size_t bytes, float gain) {
    if (!buffer || bytes == 0) return;
    if (std::fabs(gain - 1.0f) < 0.000001f) return;

    float* samples = reinterpret_cast<float*>(buffer);
    size_t sampleCount = bytes / sizeof(float);
    for (size_t i = 0; i < sampleCount; ++i) {
        samples[i] = std::clamp(samples[i] * gain, -1.0f, 1.0f);
    }
}
}

WasapiRenderSink::WasapiRenderSink() {
    m_renderEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

WasapiRenderSink::~WasapiRenderSink() {
    Stop();
    if (m_renderEvent) CloseHandle(m_renderEvent);
}

void WasapiRenderSink::SetGainDb(float gainDb) {
    if (!std::isfinite(gainDb)) gainDb = 0.0f;
    gainDb = std::clamp(gainDb, MinGainDb, MaxGainDb);
    float linear = std::pow(10.0f, gainDb / 20.0f);
    m_gainLinear.store(linear, std::memory_order_release);
}

void WasapiRenderSink::SetMicSource(RingBuffer* micRingBuffer, const WAVEFORMATEX* micFormat) {
    if (micFormat) {
        m_micSourceFormat = *micFormat;
        m_micFormatSet.store(true, std::memory_order_release);
    }
    m_micRingBuffer.store(micRingBuffer, std::memory_order_release);
}

void WasapiRenderSink::ClearMicSource() {
    m_micRingBuffer.store(nullptr, std::memory_order_release);
    m_micFormatSet.store(false, std::memory_order_release);
}

void WasapiRenderSink::SetMicGainDb(float gainDb) {
    if (!std::isfinite(gainDb)) gainDb = 0.0f;
    gainDb = std::clamp(gainDb, MinGainDb, MaxGainDb);
    float linear = std::pow(10.0f, gainDb / 20.0f);
    m_micGainLinear.store(linear, std::memory_order_release);
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

    {
        UINT32 bufFrames = 0;
        m_audioClient->GetBufferSize(&bufFrames);
        size_t maxBytes = bufFrames * m_renderFormat.nBlockAlign;
        m_micReadBuf.resize(maxBytes, 0);
        m_micConvertBuf.resize(maxBytes, 0);
    }

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
            float gain = m_gainLinear.load(std::memory_order_acquire);
            ApplyGainFloat32(renderBuffer, bytesRead, gain);
        }

        RingBuffer* micRb = m_micRingBuffer.load(std::memory_order_acquire);
        if (micRb && m_micFormatSet.load(std::memory_order_acquire) && m_renderFormat.wBitsPerSample == 32) {
            bool micFmtMatch = AudioFormatConverter::FormatsMatch(&m_micSourceFormat, &m_renderFormat);
            size_t micBytesRead = 0;

            if (micFmtMatch) {
                micBytesRead = micRb->Read(m_micReadBuf.data(), bytesNeeded);
            } else {
                size_t micFrameSize = m_micSourceFormat.nBlockAlign;
                size_t micSrcBytes = numFramesAvailable * micFrameSize;
                if (micSrcBytes > m_micConvertBuf.size()) micSrcBytes = m_micConvertBuf.size();
                size_t micSrcRead = micRb->Read(m_micConvertBuf.data(), micSrcBytes);
                if (micSrcRead > 0) {
                    AudioFormatConverter::ConvertBuffer(
                        m_micConvertBuf.data(), micSrcRead, &m_micSourceFormat,
                        m_micReadBuf.data(), bytesNeeded, &m_renderFormat, &micBytesRead);
                }
            }

            if (micBytesRead > 0) {
                float micGain = m_micGainLinear.load(std::memory_order_acquire);
                float* renderSamples = reinterpret_cast<float*>(renderBuffer);
                const float* micSamples = reinterpret_cast<const float*>(m_micReadBuf.data());
                size_t mixSamples = (std::min)(bytesNeeded, micBytesRead) / sizeof(float);
                for (size_t i = 0; i < mixSamples; ++i) {
                    renderSamples[i] = std::clamp(renderSamples[i] + micSamples[i] * micGain, -1.0f, 1.0f);
                }
            }
        }

        if (bytesRead > 0 && m_renderFormat.wBitsPerSample == 32) {
            size_t sampleCount = bytesNeeded / sizeof(float);
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
