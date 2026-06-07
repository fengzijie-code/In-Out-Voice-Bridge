#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <atomic>
#include <thread>

class RingBuffer;

class MicCapture {
public:
    MicCapture();
    ~MicCapture();

    MicCapture(const MicCapture&) = delete;
    MicCapture& operator=(const MicCapture&) = delete;

    HRESULT Start(LPCWSTR deviceId, const WAVEFORMATEX* desiredFormat, RingBuffer* ringBuffer);
    void Stop();
    bool IsCapturing() const { return m_capturing.load(); }
    float GetRmsLevel() const { return m_rmsLevel.load(); }
    WAVEFORMATEX GetCaptureFormat() const { return m_captureFormat; }

private:
    void CaptureThread();

    IMMDevice* m_device = nullptr;
    IAudioClient* m_audioClient = nullptr;
    IAudioCaptureClient* m_captureClient = nullptr;
    HANDLE m_captureEvent = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_capturing{ false };
    std::atomic<float> m_rmsLevel{ 0.0f };
    RingBuffer* m_ringBuffer = nullptr;
    WAVEFORMATEX m_captureFormat{};
};
