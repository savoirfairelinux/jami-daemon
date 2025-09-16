#pragma once

#ifndef interface
#define interface struct
#endif

#include <AudioClient.h>
#include <mmdeviceapi.h>
#include <initguid.h>
#include <guiddef.h>
#include <mfapi.h>
#include <functional>

#include <winrt/Windows.Foundation.h>
#include <wil\com.h>
#include <wil\result.h>

#include "Common.h"

class CLoopbackCapture : public winrt::implements<CLoopbackCapture,
                                                  IMFAsyncCallback,
                                                  IActivateAudioInterfaceCompletionHandler,
                                                  IUnknown>
{
public:
    // Callback type for audio data
    using AudioDataCallback = std::function<void(const BYTE* audioData,
                                                 UINT32 framesAvailable,
                                                 DWORD captureFlags,
                                                 UINT64 devicePosition,
                                                 UINT64 qpcPosition,
                                                 const WAVEFORMATEX& format)>;

    CLoopbackCapture() = default;
    ~CLoopbackCapture();

    HRESULT StartCaptureAsync(DWORD processId, bool includeProcessTree, AudioDataCallback callback);
    HRESULT StopCaptureAsync();

    METHODASYNCCALLBACK(CLoopbackCapture, StartCapture, OnStartCapture);
    METHODASYNCCALLBACK(CLoopbackCapture, StopCapture, OnStopCapture);
    METHODASYNCCALLBACK(CLoopbackCapture, SampleReady, OnSampleReady);
    METHODASYNCCALLBACK(CLoopbackCapture, FinishCapture, OnFinishCapture);

    // IActivateAudioInterfaceCompletionHandler
    STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation);

private:
    // NB: All states >= Initialized will allow some methods
    // to be called successfully on the Audio Client
    enum class DeviceState {
        Uninitialized,
        Error,
        Initialized,
        Starting,
        Capturing,
        Stopping,
        Stopped,
    };

    HRESULT OnStartCapture(IMFAsyncResult* pResult);
    HRESULT OnStopCapture(IMFAsyncResult* pResult);
    HRESULT OnFinishCapture(IMFAsyncResult* pResult);
    HRESULT OnSampleReady(IMFAsyncResult* pResult);

    HRESULT InitializeLoopbackCapture();
    HRESULT OnAudioSampleRequested();

    HRESULT ActivateAudioInterface(DWORD processId, bool includeProcessTree);
    HRESULT FinishCaptureAsync();

    HRESULT SetDeviceStateErrorIfFailed(HRESULT hr);

    HRESULT IMFAsyncCallback::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue);
    HRESULT IMFAsyncCallback::Invoke(IMFAsyncResult* pResult);

    wil::com_ptr_nothrow<IAudioClient> m_AudioClient;
    WAVEFORMATEX m_CaptureFormat {};
    UINT32 m_BufferFrames = 0;
    wil::com_ptr_nothrow<IAudioCaptureClient> m_AudioCaptureClient;
    wil::com_ptr_nothrow<IMFAsyncResult> m_SampleReadyAsyncResult;

    wil::unique_event_nothrow m_SampleReadyEvent;
    MFWORKITEM_KEY m_SampleReadyKey = 0;
    wil::critical_section m_CritSec;
    DWORD m_dwQueueID = 0;

    // Audio data callback
    AudioDataCallback m_audioDataCallback;

    // This member is used to communicate between the main thread
    // and the ActivateCompleted callback.
    HRESULT m_activateResult = E_UNEXPECTED;

    DeviceState m_DeviceState {DeviceState::Uninitialized};
    wil::unique_event_nothrow m_hActivateCompleted;
    wil::unique_event_nothrow m_hCaptureStopped;
};