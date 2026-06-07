#include <Windows.h>
#include <combaseapi.h>
#include "ProcessLoopbackCapture.h"
#include "WasapiRenderSink.h"
#include "MicCapture.h"
#include "RingBuffer.h"

#include <mutex>
#include <memory>
#include <cstdio>
#include <cmath>
#include <algorithm>

static std::mutex g_mutex;
static std::unique_ptr<ProcessLoopbackCapture> g_capture;
static std::unique_ptr<WasapiRenderSink> g_render;
static std::unique_ptr<RingBuffer> g_ringBuffer;
static float g_gainDb = 0.0f;

static std::unique_ptr<MicCapture> g_micCapture;
static std::unique_ptr<RingBuffer> g_micRingBuffer;
static float g_micGainDb = 0.0f;

enum class BridgeState : int {
    Stopped = 0,
    Running = 1,
    Error = 2
};

static BridgeState g_state = BridgeState::Stopped;

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    return TRUE;
}

extern "C" {

constexpr float MinGainDb = -60.0f;
constexpr float MaxGainDb = 20.0f;

static float NormalizeGainDb(float gainDb) {
    if (!std::isfinite(gainDb)) return 0.0f;
    return std::clamp(gainDb, MinGainDb, MaxGainDb);
}

static void BridgeLog(const wchar_t* msg, HRESULT hr = S_OK) {
    wchar_t buf[512];
    swprintf_s(buf, L"[AudioEngine] Bridge: %s (hr=0x%08X)\n", msg, (unsigned)hr);
    OutputDebugStringW(buf);
    FILE* f = nullptr;
    _wfopen_s(&f, L"D:\\audio_bridge_debug.log", L"a");
    if (f) { fwprintf(f, L"%s", buf); fclose(f); }
}

__declspec(dllexport) HRESULT __stdcall Bridge_Start(DWORD targetPid, LPCWSTR renderDeviceId) {
    std::lock_guard<std::mutex> lock(g_mutex);

    BridgeLog(L"Bridge_Start called");
    if (g_state == BridgeState::Running) return E_FAIL;

    // ~100ms buffer at 48kHz stereo float32
    constexpr size_t BUFFER_SIZE = 48000 * 2 * sizeof(float) / 10;
    g_ringBuffer = std::make_unique<RingBuffer>(BUFFER_SIZE);
    g_capture = std::make_unique<ProcessLoopbackCapture>();
    g_render = std::make_unique<WasapiRenderSink>();
    g_render->SetGainDb(g_gainDb);

    HRESULT hr = g_capture->Start(targetPid, g_ringBuffer.get());
    BridgeLog(L"capture->Start returned", hr);
    if (FAILED(hr)) {
        g_capture.reset();
        g_render.reset();
        g_ringBuffer.reset();
        g_state = BridgeState::Error;
        return hr;
    }

    WAVEFORMATEX captureFormat = g_capture->GetCaptureFormat();
    hr = g_render->Start(renderDeviceId, &captureFormat, g_ringBuffer.get());
    BridgeLog(L"render->Start returned", hr);
    if (FAILED(hr)) {
        g_capture->Stop();
        g_capture.reset();
        g_render.reset();
        g_ringBuffer.reset();
        g_state = BridgeState::Error;
        return hr;
    }

    g_state = BridgeState::Running;
    BridgeLog(L"Bridge_Start success");
    return S_OK;
}

__declspec(dllexport) HRESULT __stdcall Bridge_Stop() {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_render) g_render->ClearMicSource();
    if (g_micCapture) g_micCapture->Stop();
    g_micCapture.reset();
    g_micRingBuffer.reset();

    if (g_capture) g_capture->Stop();
    if (g_render) g_render->Stop();

    g_capture.reset();
    g_render.reset();
    g_ringBuffer.reset();
    g_state = BridgeState::Stopped;
    return S_OK;
}

__declspec(dllexport) HRESULT __stdcall Bridge_SetGainDb(float gainDb) {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_gainDb = NormalizeGainDb(gainDb);
    if (g_render) {
        g_render->SetGainDb(g_gainDb);
    }
    BridgeLog(L"Bridge_SetGainDb", S_OK);
    return S_OK;
}

__declspec(dllexport) HRESULT __stdcall Bridge_GetLevels(float* captureRms, float* renderRms) {
    if (!captureRms || !renderRms) return E_POINTER;

    std::lock_guard<std::mutex> lock(g_mutex);
    *captureRms = g_capture ? g_capture->GetRmsLevel() : 0.0f;
    *renderRms = g_render ? g_render->GetRmsLevel() : 0.0f;
    return S_OK;
}

__declspec(dllexport) HRESULT __stdcall Bridge_GetState(int* state) {
    if (!state) return E_POINTER;
    *state = static_cast<int>(g_state);
    return S_OK;
}

__declspec(dllexport) HRESULT __stdcall Bridge_StartMic(LPCWSTR micDeviceId) {
    std::lock_guard<std::mutex> lock(g_mutex);

    BridgeLog(L"Bridge_StartMic called");
    if (g_state != BridgeState::Running) return E_FAIL;
    if (!g_render) return E_FAIL;

    if (g_micCapture) {
        g_render->ClearMicSource();
        g_micCapture->Stop();
        g_micCapture.reset();
        g_micRingBuffer.reset();
    }

    constexpr size_t BUFFER_SIZE = 48000 * 2 * sizeof(float) / 10;
    g_micRingBuffer = std::make_unique<RingBuffer>(BUFFER_SIZE);
    g_micCapture = std::make_unique<MicCapture>();

    WAVEFORMATEX renderFormat = g_render->GetRenderFormat();
    HRESULT hr = g_micCapture->Start(micDeviceId, &renderFormat, g_micRingBuffer.get());
    BridgeLog(L"micCapture->Start returned", hr);
    if (FAILED(hr)) {
        g_micCapture.reset();
        g_micRingBuffer.reset();
        return hr;
    }

    WAVEFORMATEX micFormat = g_micCapture->GetCaptureFormat();
    g_render->SetMicSource(g_micRingBuffer.get(), &micFormat);
    g_render->SetMicGainDb(g_micGainDb);

    BridgeLog(L"Bridge_StartMic success");
    return S_OK;
}

__declspec(dllexport) HRESULT __stdcall Bridge_StopMic() {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_render) g_render->ClearMicSource();
    if (g_micCapture) g_micCapture->Stop();
    g_micCapture.reset();
    g_micRingBuffer.reset();

    BridgeLog(L"Bridge_StopMic");
    return S_OK;
}

__declspec(dllexport) HRESULT __stdcall Bridge_SetMicGainDb(float gainDb) {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_micGainDb = NormalizeGainDb(gainDb);
    if (g_render) {
        g_render->SetMicGainDb(g_micGainDb);
    }
    BridgeLog(L"Bridge_SetMicGainDb", S_OK);
    return S_OK;
}

__declspec(dllexport) HRESULT __stdcall Bridge_GetMicLevel(float* micRms) {
    if (!micRms) return E_POINTER;

    std::lock_guard<std::mutex> lock(g_mutex);
    *micRms = g_micCapture ? g_micCapture->GetRmsLevel() : 0.0f;
    return S_OK;
}

} // extern "C"
