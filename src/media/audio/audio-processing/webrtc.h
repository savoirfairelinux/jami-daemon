/*
 *  Copyright (C) 2021-2022 Savoir-faire Linux Inc.
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

#include "audio_processor.h"
#include "audio/audio_frame_resizer.h"

#include <memory>

namespace webrtc {
class AudioProcessing;
}

namespace jami {

class WebRTCAudioProcessor final : public AudioProcessor
{
public:
    WebRTCAudioProcessor(AudioFormat format, unsigned frameSize);
    ~WebRTCAudioProcessor() = default;

    // Inherited via AudioProcessor
    std::shared_ptr<AudioFrame> getProcessed() override;

    void enableEchoCancel(bool enabled) override;
    void enableNoiseSuppression(bool enabled) override;
    void enableAutomaticGainControl(bool enabled) override;

private:
    std::unique_ptr<webrtc::AudioProcessing> apm;

    using fChannelBuffer = std::vector<std::vector<float>>;
    fChannelBuffer fRecordBuffer_;
    fChannelBuffer fPlaybackBuffer_;
    AudioBuffer iRecordBuffer_;
    AudioBuffer iPlaybackBuffer_;
    int analogLevel_ {0};

    // artificially extend voice activity by this long, milliseconds
    unsigned int forceMinimumVoiceActivityMs;

    // current number of frames to force the voice activity to be true
    unsigned int forceVoiceActiveFramesLeft {0};

    // voice activity must be active for this long _before_ it is considered legitimate
    // milliseconds
    unsigned int minimumConsequtiveDurationMs;

    // current number of frames that the voice activity has been true
    unsigned int consecutiveActiveFrames {0};
};
} // namespace jami
