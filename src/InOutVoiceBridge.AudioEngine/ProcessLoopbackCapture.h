#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <atomic>
#include <thread>
#include <functional>

class RingBuffer;

class ProcessLoopbackCapture {
public:
    ProcessLoopbackCapture();
    ~ProcessLoopbackCapture();

    ProcessLoopbackCapture(const ProcessLoopbackCapture&) = delete;
    ProcessLoopbackCapture& operator=(const ProcessLoopbackCapture&) = delete;

    HRESULT Start(DWORD processId, RingBuffer* ringBuffer);
    void Stop();
    bool IsCapturing() const { return m_capturing.load(); }
    float GetRmsLevel() const { return m_rmsLevel.load(); }
    WAVEFORMATEX GetCaptureFormat() const { return m_captureFormat; }

private:
    void CaptureThread();

    IAudioClient* m_audioClient = nullptr;
    IAudioCaptureClient* m_captureClient = nullptr;
    HANDLE m_captureEvent = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_capturing{ false };
    std::atomic<float> m_rmsLevel{ 0.0f };
    RingBuffer* m_ringBuffer = nullptr;
    WAVEFORMATEX m_captureFormat{};
};
