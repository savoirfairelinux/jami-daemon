/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>
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

#include "audio/echo-cancel/echo_canceller.h"
#include "audio/audio_frame_resizer.h"

#include <memory>

namespace jami {

class WebRTCEchoCanceller final : public EchoCanceller
{
public:
    WebRTCEchoCanceller(AudioFormat format, unsigned frameSize);
    ~WebRTCEchoCanceller() = default;

    // Inherited via EchoCanceller
    void putRecorded(std::shared_ptr<AudioFrame>&& buf) override;
    void putPlayback(const std::shared_ptr<AudioFrame>& buf) override;
    std::shared_ptr<AudioFrame> getProcessed() override;
    void done() override;

private:
    struct WebRTCAPMImpl;
    std::unique_ptr<WebRTCAPMImpl> pimpl_;

    using fChannelBuffer = std::vector<std::vector<float>>;
    fChannelBuffer fRecordBuffer_;
    fChannelBuffer fPlaybackBuffer_;
    AudioBuffer iRecordBuffer_;
    AudioBuffer iPlaybackBuffer_;
    int analogLevel_ {0};
};
} // namespace jami
