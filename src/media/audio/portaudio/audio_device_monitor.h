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

#include <atlbase.h>
#include <conio.h>
#include <mmdeviceapi.h>
#include <string>
#include <windows.h>
#include <Functiondiscoverykeys_devpkey.h>

#include <functional>

enum class DeviceEventType { BecameActive, BecameInactive };
using DeviceEventCallback
    = std::function<void(const std::string& deviceName, const DeviceEventType event)>;

std::string
GetFriendlyNameFromIMMDeviceId(LPCWSTR deviceId)
{
    CComPtr<IMMDeviceEnumerator> deviceEnumerator;
    CComPtr<IMMDevice> device;
    CComPtr<IPropertyStore> props;

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                nullptr,
                                CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                (void**) &deviceEnumerator)))
        return {};

    if (FAILED(deviceEnumerator->GetDevice(deviceId, &device)))
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
        // Fill our list of active devices
        enumerateDevices();
    }

    void setDeviceEventCallback(DeviceEventCallback callback) { deviceEventCallback_ = callback; }

    void enumerateDevices()
    {
        CComPtr<IMMDeviceEnumerator> enumerator;
        CComPtr<IMMDeviceCollection> devices;
        CComPtr<IMMDevice> device;
        UINT count = 0;

        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                    nullptr,
                                    CLSCTX_ALL,
                                    __uuidof(IMMDeviceEnumerator),
                                    (void**) &enumerator))) {
            return;
        }

        if (FAILED(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &devices))) {
            return;
        }

        if (FAILED(devices->GetCount(&count))) {
            return;
        }

        for (UINT i = 0; i < count; ++i) {
            if (FAILED(devices->Item(i, &device))) {
                continue;
            }
            LPWSTR deviceId = nullptr;
            if (FAILED(device->GetId(&deviceId))) {
                continue;
            }
            auto deviceName = GetFriendlyNameFromIMMDeviceId(deviceId);
            activeDevices_.push_back(deviceName);
            CoTaskMemFree(deviceId);
        }
    }

    void handleStateChanged(LPCWSTR deviceId, DWORD newState)
    {
        auto active = static_cast<int>(newState) == DEVICE_STATE_ACTIVE;
        auto deviceName = GetFriendlyNameFromIMMDeviceId(deviceId);
        // Check if this device has changed state
        auto it = std::find(activeDevices_.begin(), activeDevices_.end(), deviceName);
        if (active && it == activeDevices_.end()) {
            // Device is active and not in our list, add it
            activeDevices_.push_back(deviceName);
            deviceEventCallback_(deviceName, DeviceEventType::BecameActive);
        } else if (!active && it != activeDevices_.end()) {
            // Device is inactive and in our list, remove it
            activeDevices_.erase(it);
            deviceEventCallback_(deviceName, DeviceEventType::BecameInactive);
        }
    }

    // IUnknown methods
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&_refCount); }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG ulRef = InterlockedDecrement(&_refCount);
        if (ulRef == 0) {
            delete this;
        }
        return ulRef;
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
        if (!deviceEventCallback_) {
            return S_OK;
        }
        handleStateChanged(deviceId, newState);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR deviceId) override
    {
        if (!deviceEventCallback_) {
            return S_OK;
        }
        handleStateChanged(deviceId, DEVICE_STATE_ACTIVE);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR deviceId) override
    {
        if (!deviceEventCallback_) {
            return S_OK;
        }
        handleStateChanged(deviceId, DEVICE_STATE_DISABLED);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow,
                                                     ERole role,
                                                     LPCWSTR deviceId) override
    {
        // If the default communication device changes, we need to restart the layer
        // to ensure the new device is used.
        if (role == eCommunications) {
            deviceEventCallback_(GetFriendlyNameFromIMMDeviceId(deviceId),
                                 DeviceEventType::BecameActive);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR deviceId,
                                                     const PROPERTYKEY key) override
    {
        // Unhandled event
        return S_OK;
    }

private:
    // The callback that will be called when a device state changes
    DeviceEventCallback deviceEventCallback_;

    // A list of active device we keep to avoid sending duplicate events
    std::vector<std::string> activeDevices_;
};
