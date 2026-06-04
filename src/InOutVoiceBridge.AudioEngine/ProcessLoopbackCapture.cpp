#include "ProcessLoopbackCapture.h"
#include "RingBuffer.h"
#include "AudioFormatConverter.h"

#include <audioclientactivationparams.h>
#include <combaseapi.h>
#include <avrt.h>
#include <cmath>
#include <cstdio>

#pragma comment(lib, "avrt.lib")

struct ActivationCompletionHandler : public IActivateAudioInterfaceCompletionHandler {
    HANDLE completionEvent;
    IActivateAudioInterfaceAsyncOperation* operation = nullptr;
    LONG refCount = 1;

    ActivationCompletionHandler() {
        completionEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    }
    ~ActivationCompletionHandler() {
        CloseHandle(completionEvent);
    }

    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IActivateAudioInterfaceCompletionHandler) || riid == __uuidof(IAgileObject)) {
            *ppv = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return InterlockedIncrement(&refCount); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG c = InterlockedDecrement(&refCount);
        if (c == 0) delete this;
        return c;
    }
    STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* op) override {
        operation = op;
        op->AddRef();
        SetEvent(completionEvent);
        return S_OK;
    }
};

ProcessLoopbackCapture::ProcessLoopbackCapture() {
    m_captureEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

ProcessLoopbackCapture::~ProcessLoopbackCapture() {
    Stop();
    if (m_captureEvent) CloseHandle(m_captureEvent);
}

static void DbgLog(const wchar_t* msg, HRESULT hr = S_OK) {
    wchar_t buf[512];
    swprintf_s(buf, L"[AudioEngine] %s (hr=0x%08X)\n", msg, (unsigned)hr);
    OutputDebugStringW(buf);
    FILE* f = nullptr;
    _wfopen_s(&f, L"D:\\audio_bridge_debug.log", L"a");
    if (f) { fwprintf(f, L"%s", buf); fclose(f); }
}

HRESULT ProcessLoopbackCapture::Start(DWORD processId, RingBuffer* ringBuffer) {
    if (m_capturing.load()) return E_FAIL;
    m_ringBuffer = ringBuffer;

    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    DbgLog(L"CoInitializeEx(MTA)", hrCom);

    DbgLog(L"Capture::Start - begin");

    AUDIOCLIENT_ACTIVATION_PARAMS audioclientActivationParams = {};
    audioclientActivationParams.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    audioclientActivationParams.ProcessLoopbackParams.TargetProcessId = processId;
    audioclientActivationParams.ProcessLoopbackParams.ProcessLoopbackMode =
        PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

    PROPVARIANT activateParams = {};
    activateParams.vt = VT_BLOB;
    activateParams.blob.cbSize = sizeof(audioclientActivationParams);
    activateParams.blob.pBlobData = reinterpret_cast<BYTE*>(&audioclientActivationParams);

    auto handler = new ActivationCompletionHandler();
    IActivateAudioInterfaceAsyncOperation* asyncOp = nullptr;

    HRESULT hr = ActivateAudioInterfaceAsync(
        VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
        __uuidof(IAudioClient),
        &activateParams,
        handler,
        &asyncOp);

    DbgLog(L"ActivateAudioInterfaceAsync", hr);
    if (FAILED(hr)) {
        handler->Release();
        if (asyncOp) asyncOp->Release();
        return hr;
    }

    DWORD waitResult = WaitForSingleObject(handler->completionEvent, 5000);
    DbgLog(L"WaitForSingleObject", waitResult);

    HRESULT activateResult = E_FAIL;
    IUnknown* activatedInterface = nullptr;

    if (handler->operation) {
        hr = handler->operation->GetActivateResult(&activateResult, &activatedInterface);
        DbgLog(L"GetActivateResult hr", hr);
        DbgLog(L"GetActivateResult activateResult", activateResult);
        handler->operation->Release();
    } else {
        DbgLog(L"handler->operation is NULL");
    }
    handler->Release();
    if (asyncOp) asyncOp->Release();

    if (FAILED(hr) || FAILED(activateResult)) {
        if (activatedInterface) activatedInterface->Release();
        return FAILED(hr) ? hr : activateResult;
    }

    hr = activatedInterface->QueryInterface(__uuidof(IAudioClient), (void**)&m_audioClient);
    activatedInterface->Release();
    DbgLog(L"QueryInterface IAudioClient", hr);
    if (FAILED(hr)) return hr;

    WAVEFORMATEX* mixFormat = nullptr;
    hr = m_audioClient->GetMixFormat(&mixFormat);
    DbgLog(L"GetMixFormat on loopback client", hr);

    bool ownedFormat = false;
    if (FAILED(hr)) {
        DbgLog(L"GetMixFormat failed, querying default render device format");
        IMMDeviceEnumerator* devEnum = nullptr;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), (void**)&devEnum);
        if (SUCCEEDED(hr)) {
            IMMDevice* defDevice = nullptr;
            hr = devEnum->GetDefaultAudioEndpoint(eRender, eConsole, &defDevice);
            if (SUCCEEDED(hr)) {
                IAudioClient* defClient = nullptr;
                hr = defDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&defClient);
                if (SUCCEEDED(hr)) {
                    hr = defClient->GetMixFormat(&mixFormat);
                    DbgLog(L"Default device GetMixFormat", hr);
                    defClient->Release();
                }
                defDevice->Release();
            }
            devEnum->Release();
        }
        if (FAILED(hr)) return hr;
    }

    {
        wchar_t fmtBuf[256];
        swprintf_s(fmtBuf, L"Format: tag=%u ch=%u rate=%u bits=%u blockAlign=%u cbSize=%u",
            mixFormat->wFormatTag, mixFormat->nChannels, mixFormat->nSamplesPerSec,
            mixFormat->wBitsPerSample, mixFormat->nBlockAlign, mixFormat->cbSize);
        DbgLog(fmtBuf);
    }

    WAVEFORMATEX* closestMatch = nullptr;
    hr = m_audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, mixFormat, &closestMatch);
    DbgLog(L"IsFormatSupported", hr);
    if (closestMatch) {
        wchar_t fmtBuf2[256];
        swprintf_s(fmtBuf2, L"ClosestMatch: tag=%u ch=%u rate=%u bits=%u blockAlign=%u cbSize=%u",
            closestMatch->wFormatTag, closestMatch->nChannels, closestMatch->nSamplesPerSec,
            closestMatch->wBitsPerSample, closestMatch->nBlockAlign, closestMatch->cbSize);
        DbgLog(fmtBuf2);
    }

    WAVEFORMATEX* formatToUse = mixFormat;
    if (hr == S_FALSE && closestMatch) {
        formatToUse = closestMatch;
        DbgLog(L"Using closestMatch format");
    }

    m_captureFormat = *formatToUse;

    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        0, 0, formatToUse, nullptr);
    DbgLog(L"IAudioClient::Initialize", hr);

    CoTaskMemFree(mixFormat);
    if (closestMatch) CoTaskMemFree(closestMatch);
    if (FAILED(hr)) return hr;

    hr = m_audioClient->SetEventHandle(m_captureEvent);
    DbgLog(L"SetEventHandle", hr);
    if (FAILED(hr)) return hr;

    hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_captureClient);
    DbgLog(L"GetService IAudioCaptureClient", hr);
    if (FAILED(hr)) return hr;

    m_capturing.store(true);
    hr = m_audioClient->Start();
    DbgLog(L"IAudioClient::Start", hr);
    if (FAILED(hr)) {
        m_capturing.store(false);
        return hr;
    }

    m_thread = std::thread(&ProcessLoopbackCapture::CaptureThread, this);
    DbgLog(L"Capture::Start - success");
    return S_OK;
}

void ProcessLoopbackCapture::Stop() {
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
    m_rmsLevel.store(0.0f);
}

void ProcessLoopbackCapture::CaptureThread() {
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
