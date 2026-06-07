#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <atomic>
#include <thread>
#include <vector>

class RingBuffer;

class WasapiRenderSink {
public:
    WasapiRenderSink();
    ~WasapiRenderSink();

    WasapiRenderSink(const WasapiRenderSink&) = delete;
    WasapiRenderSink& operator=(const WasapiRenderSink&) = delete;

    HRESULT Start(LPCWSTR deviceId, const WAVEFORMATEX* sourceFormat, RingBuffer* ringBuffer);
    void Stop();
    void SetGainDb(float gainDb);
    bool IsRendering() const { return m_rendering.load(); }
    float GetRmsLevel() const { return m_rmsLevel.load(); }
    WAVEFORMATEX GetRenderFormat() const { return m_renderFormat; }

    void SetMicSource(RingBuffer* micRingBuffer, const WAVEFORMATEX* micFormat);
    void ClearMicSource();
    void SetMicGainDb(float gainDb);

private:
    void RenderThread();

    IMMDevice* m_device = nullptr;
    IAudioClient* m_audioClient = nullptr;
    IAudioRenderClient* m_renderClient = nullptr;
    HANDLE m_renderEvent = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_rendering{ false };
    std::atomic<float> m_rmsLevel{ 0.0f };
    std::atomic<float> m_gainLinear{ 1.0f };
    RingBuffer* m_ringBuffer = nullptr;
    WAVEFORMATEX m_renderFormat{};
    WAVEFORMATEX m_sourceFormat{};

    std::atomic<RingBuffer*> m_micRingBuffer{ nullptr };
    std::atomic<float> m_micGainLinear{ 1.0f };
    WAVEFORMATEX m_micSourceFormat{};
    std::atomic<bool> m_micFormatSet{ false };
    std::vector<uint8_t> m_micReadBuf;
    std::vector<uint8_t> m_micConvertBuf;
};
