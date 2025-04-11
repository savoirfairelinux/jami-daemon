/*
 *  Copyright (C) 2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "string_utils.h"
#include "logger.h"

#include <atlbase.h>
#include <conio.h>
#include <mmdeviceapi.h>
#include <string>
#include <windows.h>
#include <Functiondiscoverykeys_devpkey.h>

#include <functional>
#include <mutex>
#include <unordered_set>

// Note: the granularity of these events is not currently required because we end up
// restarting the audio layer regardless of the event type. However, let's keep it like
// this for now in case a future PortAudio bump or layer improvement can make use of it
// to avoid unnecessary stream restarts.
enum class DeviceEventType { BecameActive, BecameInactive, DefaultChanged };
inline std::string
to_string(DeviceEventType type)
{
    switch (type) {
    case DeviceEventType::BecameActive:
        return "BecameActive";
    case DeviceEventType::BecameInactive:
        return "BecameInactive";
    case DeviceEventType::DefaultChanged:
        return "DefaultChanged";
    }
    return "Unknown";
}
using DeviceEventCallback = std::function<void(const std::string& deviceName, DeviceEventType)>;

std::string
GetFriendlyNameFromIMMDeviceId(CComPtr<IMMDeviceEnumerator> enumerator, LPCWSTR deviceId)
{
    if (!enumerator || !deviceId)
        return {};

    CComPtr<IMMDevice> device;
    CComPtr<IPropertyStore> props;

    if (FAILED(enumerator->GetDevice(deviceId, &device)))
        return {};

    if (FAILED(device->OpenPropertyStore(STGM_READ, &props)))
        return {};

    PROPVARIANT varName;
    PropVariantInit(&varName);
    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
        std::wstring name = varName.pwszVal;
        PropVariantClear(&varName);
        return jami::to_string(name);
    }

    return {};
}

class AudioDeviceNotificationClient : public IMMNotificationClient
{
    LONG _refCount;

public:
    AudioDeviceNotificationClient()
        : _refCount(1)
    {
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                    nullptr,
                                    CLSCTX_ALL,
                                    __uuidof(IMMDeviceEnumerator),
                                    (void**) &deviceEnumerator_))) {
            JAMI_ERR("Failed to create device enumerator");
        } else {
            // Fill our list of active devices (render + capture)
            enumerateDevices();
        }
    }

    ~AudioDeviceNotificationClient()
    {
        if (deviceEnumerator_) {
            deviceEnumerator_->UnregisterEndpointNotificationCallback(this);
        }
    }

    void setDeviceEventCallback(DeviceEventCallback callback)
    {
        if (!deviceEnumerator_) {
            JAMI_ERR("Device enumerator not initialized");
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            deviceEventCallback_ = callback;
        }

        // Now we can start monitoring
        deviceEnumerator_->RegisterEndpointNotificationCallback(this);
    }

    // // IUnknown methods
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        auto count = InterlockedIncrement(&_refCount);
        return count;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        auto count = InterlockedDecrement(&_refCount);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
            *ppv = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    // IMMNotificationClient methods
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR deviceId, DWORD newState) override
    {
        handleStateChanged(deviceId, newState);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR deviceId) override
    {
        // Treat addition as transition to active - we'll validate the state in handler
        handleStateChanged(deviceId, DEVICE_STATE_ACTIVE);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR deviceId) override
    {
        // Removed devices should be treated as inactive/unavailable
        handleStateChanged(deviceId, DEVICE_STATE_NOTPRESENT);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow,
                                                     ERole role,
                                                     LPCWSTR deviceId) override
    {
        // If the default communication device changes, we should restart the layer
        // to ensure the new device is used. We only care about the default communication
        // device, so we ignore other roles.
        if (role == eCommunications && deviceId && deviceEventCallback_) {
            std::string friendlyName = GetFriendlyNameFromIMMDeviceId(deviceEnumerator_, deviceId);
            deviceEventCallback_(friendlyName, DeviceEventType::DefaultChanged);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR deviceId,
                                                     const PROPERTYKEY key) override
    {
        // Currently unused - could be hooked to detect renames, etc.
        return S_OK;
    }

private:
    CComPtr<IMMDeviceEnumerator> deviceEnumerator_;

    DeviceEventCallback deviceEventCallback_;
    std::unordered_set<std::wstring> activeDevices_;

    // Notifications are invoked on system-managed threads, so we need a lock
    // to protect deviceEventCallback_ and activeDevices_
    std::mutex mutex_;

    // Enumerates both render (playback) and capture (recording) devices
    void enumerateDevices()
    {
        if (!deviceEnumerator_)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        activeDevices_.clear();

        enumerateDevicesOfType(eRender);
        enumerateDevicesOfType(eCapture);
    }

    void enumerateDevicesOfType(EDataFlow flow)
    {
        CComPtr<IMMDeviceCollection> devices;
        CComPtr<IMMDevice> device;
        UINT deviceCount = 0;

        if (FAILED(deviceEnumerator_->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &devices))) {
            JAMI_ERR("Failed to enumerate devices");
            return;
        }

        if (FAILED(devices->GetCount(&deviceCount))) {
            JAMI_ERR("Failed to get device count");
            return;
        }

        for (UINT i = 0; i < deviceCount; ++i) {
            if (FAILED(devices->Item(i, &device)))
                continue;

            LPWSTR deviceId = nullptr;
            if (FAILED(device->GetId(&deviceId)))
                continue;

            DWORD deviceState = 0;
            if (SUCCEEDED(device->GetState(&deviceState)) && deviceState == DEVICE_STATE_ACTIVE) {
                activeDevices_.insert(deviceId);
            }

            CoTaskMemFree(deviceId);
        }
    }

    void handleStateChanged(LPCWSTR deviceId, DWORD newState)
    {
        if (!deviceId || !deviceEnumerator_ || !deviceEventCallback_)
            return;

        std::lock_guard<std::mutex> lock(mutex_);

        std::wstring wsId(deviceId);
        std::string friendlyName = GetFriendlyNameFromIMMDeviceId(deviceEnumerator_, deviceId);

        // We only care if a device is entering or leaving the active state
        bool isActive = (newState == DEVICE_STATE_ACTIVE);
        auto it = activeDevices_.find(wsId);

        if (isActive && it == activeDevices_.end()) {
            // Device is active and not in our list? Add it and notify
            activeDevices_.insert(wsId);
            deviceEventCallback_(friendlyName, DeviceEventType::BecameActive);
        } else if (!isActive && it != activeDevices_.end()) {
            // Device is inactive and in our list? Remove it and notify
            activeDevices_.erase(it);
            deviceEventCallback_(friendlyName, DeviceEventType::BecameInactive);
        }
    }
};

// RAII COM wrapper for AudioDeviceNotificationClient
struct AudioDeviceNotificationClient_deleter
{
    void operator()(AudioDeviceNotificationClient* client) const { client->Release(); }
};

typedef std::unique_ptr<AudioDeviceNotificationClient, AudioDeviceNotificationClient_deleter>
    AudioDeviceNotificationClientPtr;
