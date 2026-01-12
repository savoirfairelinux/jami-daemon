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
#include <wil/com.h>
#include <wil/result.h>

#include "Common.h"

class WasapiLoopbackCapture : public winrt::implements<WasapiLoopbackCapture,
                                                  IMFAsyncCallback,
                                                  IActivateAudioInterfaceCompletionHandler,
                                                  IUnknown>
{
public:
    using AudioDataCallback = std::function<void(const BYTE* audioData,
                                                 UINT32 framesAvailable,
                                                 DWORD captureFlags,
                                                 UINT64 devicePosition,
                                                 UINT64 qpcPosition,
                                                 const WAVEFORMATEX& format)>;

    WasapiLoopbackCapture() = default;
    ~WasapiLoopbackCapture();

    /**
     * Start capturing audio data from the given PID.
     * @param processId The process ID to capture audio from
     * @param includeProcessTree Whether to include child processes of the given PID
     * @param callback The callback to be invoked when audio data is available
     * @return HRESULT indicating success or failure
     */
    HRESULT StartCaptureAsync(DWORD processId, bool includeProcessTree, AudioDataCallback callback);

    /**
     * Stop capture asynchronously via MF Work Item
     * @return HRESULT indicating success or failure
     */
    HRESULT StopCaptureAsync();

    METHODASYNCCALLBACK(WasapiLoopbackCapture, StartCapture, OnStartCapture);
    METHODASYNCCALLBACK(WasapiLoopbackCapture, StopCapture, OnStopCapture);
    METHODASYNCCALLBACK(WasapiLoopbackCapture, SampleReady, OnSampleReady);
    METHODASYNCCALLBACK(WasapiLoopbackCapture, FinishCapture, OnFinishCapture);

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

    // Callback method to start capture
    HRESULT OnStartCapture(IMFAsyncResult* pResult);

    // Callback method to stop capture
    HRESULT OnStopCapture(IMFAsyncResult* pResult);

    // Callback method to finalize capture
    HRESULT OnFinishCapture(IMFAsyncResult* pResult);

    //  Callback method when ready to fill sample buffer
    HRESULT OnSampleReady(IMFAsyncResult* pResult);

    HRESULT InitializeLoopbackCapture();

    //  Called when audio device fires m_SampleReadyEvent
    HRESULT OnAudioSampleRequested();

    HRESULT ActivateAudioInterface(DWORD processId, bool includeProcessTree);

    //  Finalizes capture on a separate thread via MF Work Item
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

    AudioDataCallback m_audioDataCallback;

    // This member is used to communicate between the main thread
    // and the ActivateCompleted callback.
    HRESULT m_activateResult = E_UNEXPECTED;

    DeviceState m_DeviceState {DeviceState::Uninitialized};
    wil::unique_event_nothrow m_hActivateCompleted;
    wil::unique_event_nothrow m_hCaptureStopped;
};