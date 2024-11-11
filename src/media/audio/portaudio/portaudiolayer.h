/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#pragma once

#include "audio/audiolayer.h"
#include "noncopyable.h"

#include <memory>
#include <array>

namespace jami {

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
};

} // namespace jami
