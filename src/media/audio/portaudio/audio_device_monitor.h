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

std::wstring
GetFriendlyNameFromIMMDeviceId(LPCWSTR deviceId)
{
    CComPtr<IMMDeviceEnumerator> enumerator;
    CComPtr<IMMDevice> device;
    CComPtr<IPropertyStore> props;

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                nullptr,
                                CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                (void**) &enumerator)))
        return L"";

    if (FAILED(enumerator->GetDevice(deviceId, &device)))
        return L"";

    if (FAILED(device->OpenPropertyStore(STGM_READ, &props)))
        return L"";

    PROPVARIANT varName;
    PropVariantInit(&varName);
    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
        std::wstring name = varName.pwszVal;
        PropVariantClear(&varName);
        return name;
    }

    return L"";
}

class AudioDeviceNotificationClient : public IMMNotificationClient
{
    LONG _refCount;

public:
    AudioDeviceNotificationClient()
        : _refCount(1)
    {}

    void setDeviceEventCallback(DeviceEventCallback callback) { deviceEventCallback_ = callback; }

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
        // We are only interested in knowing the device is active or not
        // mmdeviceapi.h defines the following states:
        //  - DEVICE_STATE_ACTIVE      0x00000001
        //  - DEVICE_STATE_DISABLED    0x00000002
        //  - DEVICE_STATE_NOTPRESENT  0x00000004
        //  - DEVICE_STATE_UNPLUGGED   0x00000008
        deviceEventCallback_(jami::to_string(GetFriendlyNameFromIMMDeviceId(deviceId)),
                             newState == DEVICE_STATE_ACTIVE ? DeviceEventType::BecameActive
                                                             : DeviceEventType::BecameInactive);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR deviceId) override
    {
        if (!deviceEventCallback_) {
            return S_OK;
        }
        deviceEventCallback_(jami::to_string(GetFriendlyNameFromIMMDeviceId(deviceId)),
                             DeviceEventType::BecameActive);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR deviceId) override
    {
        if (!deviceEventCallback_) {
            return S_OK;
        }
        deviceEventCallback_(jami::to_string(GetFriendlyNameFromIMMDeviceId(deviceId)),
                             DeviceEventType::BecameInactive);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow,
                                                     ERole role,
                                                     LPCWSTR deviceId) override
    {
        // TODO: We may want to handle this event to know when the default device changes
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
};
