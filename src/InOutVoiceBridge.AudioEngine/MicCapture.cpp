#include "MicCapture.h"
#include "RingBuffer.h"
#include "AudioFormatConverter.h"

#include <avrt.h>
#include <cmath>
#include <cstdio>
#include <vector>

#pragma comment(lib, "avrt.lib")

static void MicLog(const wchar_t* msg, HRESULT hr = S_OK) {
    wchar_t buf[512];
    swprintf_s(buf, L"[AudioEngine] MicCapture: %s (hr=0x%08X)\n", msg, (unsigned)hr);
    OutputDebugStringW(buf);
    FILE* f = nullptr;
    _wfopen_s(&f, L"D:\\audio_bridge_debug.log", L"a");
    if (f) { fwprintf(f, L"%s", buf); fclose(f); }
}

MicCapture::MicCapture() {
    m_captureEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

MicCapture::~MicCapture() {
    Stop();
    if (m_captureEvent) CloseHandle(m_captureEvent);
}

HRESULT MicCapture::Start(LPCWSTR deviceId, const WAVEFORMATEX* desiredFormat, RingBuffer* ringBuffer) {
    if (m_capturing.load()) return E_FAIL;
    m_ringBuffer = ringBuffer;

    MicLog(L"MicCapture::Start - begin");

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) {
        MicLog(L"CoCreateInstance MMDeviceEnumerator failed", hr);
        return hr;
    }

    hr = enumerator->GetDevice(deviceId, &m_device);
    enumerator->Release();
    if (FAILED(hr)) {
        MicLog(L"GetDevice failed", hr);
        return hr;
    }

    hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_audioClient);
    if (FAILED(hr)) {
        MicLog(L"Activate IAudioClient failed", hr);
        return hr;
    }

    WAVEFORMATEX* mixFormat = nullptr;
    hr = m_audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr)) {
        MicLog(L"GetMixFormat failed", hr);
        return hr;
    }

    WAVEFORMATEX* formatToUse = mixFormat;
    WAVEFORMATEX* closestMatch = nullptr;

    if (desiredFormat) {
        hr = m_audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, desiredFormat, &closestMatch);
        if (hr == S_OK) {
            formatToUse = const_cast<WAVEFORMATEX*>(desiredFormat);
            MicLog(L"Using desired format (render device format)");
        } else if (hr == S_FALSE && closestMatch) {
            formatToUse = closestMatch;
            MicLog(L"Using closest match to desired format");
        } else {
            MicLog(L"Desired format not supported, using mic native format");
        }
    }

    {
        wchar_t fmtBuf[256];
        swprintf_s(fmtBuf, L"Mic format: tag=%u ch=%u rate=%u bits=%u blockAlign=%u",
            formatToUse->wFormatTag, formatToUse->nChannels, formatToUse->nSamplesPerSec,
            formatToUse->wBitsPerSample, formatToUse->nBlockAlign);
        MicLog(fmtBuf);
    }

    m_captureFormat = *formatToUse;

    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        0, 0, formatToUse, nullptr);

    CoTaskMemFree(mixFormat);
    if (closestMatch) CoTaskMemFree(closestMatch);

    if (FAILED(hr)) {
        MicLog(L"IAudioClient::Initialize failed", hr);
        return hr;
    }

    hr = m_audioClient->SetEventHandle(m_captureEvent);
    if (FAILED(hr)) {
        MicLog(L"SetEventHandle failed", hr);
        return hr;
    }

    hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_captureClient);
    if (FAILED(hr)) {
        MicLog(L"GetService IAudioCaptureClient failed", hr);
        return hr;
    }

    m_capturing.store(true);
    hr = m_audioClient->Start();
    if (FAILED(hr)) {
        m_capturing.store(false);
        MicLog(L"IAudioClient::Start failed", hr);
        return hr;
    }

    m_thread = std::thread(&MicCapture::CaptureThread, this);
    MicLog(L"MicCapture::Start - success");
    return S_OK;
}

void MicCapture::Stop() {
    m_capturing.store(false);
    if (m_thread.joinable()) {
        SetEvent(m_captureEvent);
        m_thread.join();
    }
    if (m_audioClient) {
        m_audioClient->Stop();
        m_audioClient->Release();
        m_audioClient = nullptr;
    }
    if (m_captureClient) {
        m_captureClient->Release();
        m_captureClient = nullptr;
    }
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
    m_rmsLevel.store(0.0f);
}

void MicCapture::CaptureThread() {
    DWORD taskIndex = 0;
    HANDLE avrtHandle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    while (m_capturing.load()) {
        DWORD waitResult = WaitForSingleObject(m_captureEvent, 100);
        if (!m_capturing.load()) break;
        if (waitResult != WAIT_OBJECT_0) continue;

        UINT32 packetLength = 0;
        while (SUCCEEDED(m_captureClient->GetNextPacketSize(&packetLength)) && packetLength > 0) {
            BYTE* data = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;

            HRESULT hr = m_captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
            if (FAILED(hr)) break;

            size_t bytesPerFrame = m_captureFormat.nBlockAlign;
            size_t totalBytes = numFrames * bytesPerFrame;

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                std::vector<uint8_t> silence(totalBytes, 0);
                m_ringBuffer->Write(silence.data(), totalBytes);
                m_rmsLevel.store(0.0f);
            } else {
                m_ringBuffer->Write(data, totalBytes);

                if (m_captureFormat.wBitsPerSample == 32) {
                    size_t sampleCount = totalBytes / sizeof(float);
                    float rms = AudioFormatConverter::CalculateRms(
                        reinterpret_cast<const float*>(data), sampleCount);
                    m_rmsLevel.store(rms);
                }
            }

            m_captureClient->ReleaseBuffer(numFrames);
        }
    }

    if (avrtHandle) AvRevertMmThreadCharacteristics(avrtHandle);
}
