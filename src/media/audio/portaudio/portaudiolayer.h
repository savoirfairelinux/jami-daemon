/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Edric Ladent-Milaret <edric.ladent-milaret@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

#include "audio/audiolayer.h"
#include "noncopyable.h"
#include "manager.h"

#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#include <memory>
#include <array>

namespace jami {

#if !defined(SAFE_RELEASE)
#define SAFE_RELEASE(punk)  \
          if ((punk) != NULL)  \
            { (punk)->Release(); (punk) = NULL; }
#endif

class CMyNotificationClient : public IMMNotificationClient
{
    LONG _cRef;

public:
    CMyNotificationClient() :
        _cRef(1)
    {
    }

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ulRef = InterlockedDecrement(&_cRef);
        if (0 == ulRef)
        {
            delete this;
        }
        return ulRef;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(
                                REFIID riid, VOID **ppvInterface)
    {
        if (IID_IUnknown == riid)
        {
            AddRef();
            *ppvInterface = (IUnknown*)this;
        }
        else if (__uuidof(IMMNotificationClient) == riid)
        {
            AddRef();
            *ppvInterface = (IMMNotificationClient*)this;
        }
        else
        {
            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    // Called when an audio endpoint device is added
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId)
    {
        JAMI_DBG() << "************ADD EVENT*******************";
        return S_OK;
    }

    // Called when an audio endpoint device is removed
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId)
    {
        JAMI_DBG() << "************REMOVE EVENT*******************";
        return S_OK;
    }

    // Notifies that the default audio endpoint device for a particular role has changed.
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
    {
        // Implement this function here
        JAMI_DBG() << "************OnDefaultDeviceChanged*******************";
        return S_OK;
    }

    // Notifies that the state of an audio endpoint device has changed.
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
    {
        // Implement this function here
        if (dwNewState == DEVICE_STATE_ACTIVE) {
            JAMI_DBG() << "************OnDeviceStateChanged*******************";
            auto currentPlugin = Manager::instance().getCurrentAudioOutputPlugin();
            Manager::instance().setAudioPlugin(currentPlugin);
        }
        return S_OK;
    }

    // Notifies that the properties of an audio endpoint device have changed.
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
    {
        // Implement this function here
        return S_OK;
    }
};

class PortAudioLayer final : public AudioLayer
{
public:
    PortAudioLayer(const AudioPreference& pref);
    ~PortAudioLayer();

    std::vector<std::string> getCaptureDeviceList() const override;
    std::vector<std::string> getPlaybackDeviceList() const override;
    int getAudioDeviceIndex(const std::string& name, AudioDeviceType type) const override;
    std::string getAudioDeviceName(int index, AudioDeviceType type) const override;
    int getIndexCapture() const override;
    int getIndexPlayback() const override;
    int getIndexRingtone() const override;

    /**
     * Start the capture stream and prepare the playback stream.
     * The playback starts accordingly to its threshold
     */
    void startStream(AudioDeviceType stream = AudioDeviceType::ALL) override;

    /**
     * Stop the playback and capture streams.
     * Drops the pending frames and put the capture and playback handles to PREPARED state
     */
    void stopStream(AudioDeviceType stream = AudioDeviceType::ALL) override;

    void updatePreference(AudioPreference& pref, int index, AudioDeviceType type) override;

private:
    NON_COPYABLE(PortAudioLayer);

    struct PortAudioLayerImpl;
    std::unique_ptr<PortAudioLayerImpl> pimpl_;

    IMMDeviceEnumerator* pDevEnumerator_ {nullptr};
    CMyNotificationClient* pNotificationClient_;
    std::thread hotplugThread_;
};

} // namespace jami
