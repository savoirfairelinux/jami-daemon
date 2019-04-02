/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#include <memory>
#include <array>

namespace jami {

class PortAudioLayer : public AudioLayer {

public:
    PortAudioLayer(const AudioPreference& pref);
    virtual ~PortAudioLayer() = default;

    virtual std::vector<std::string> getCaptureDeviceList() const;
    virtual std::vector<std::string> getPlaybackDeviceList() const;
    virtual int getAudioDeviceIndex(const std::string& name, DeviceType type) const;
    virtual std::string getAudioDeviceName(int index, DeviceType type) const;
    virtual int getIndexCapture() const;
    virtual int getIndexPlayback() const;
    virtual int getIndexRingtone() const;

    /**
     * Start the capture stream and prepare the playback stream.
     * The playback starts accordingly to its threshold
     */
    virtual void startStream();

    /**
     * Stop the playback and capture streams.
     * Drops the pending frames and put the capture and playback handles to PREPARED state
     */
    virtual void stopStream();

    virtual void updatePreference(AudioPreference& pref, int index, DeviceType type);

private:
    NON_COPYABLE(PortAudioLayer);

    struct PortAudioLayerImpl;
    std::unique_ptr<PortAudioLayerImpl> pimpl_;
};

} // namespace jami
