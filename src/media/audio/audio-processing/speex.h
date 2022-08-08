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

// typedef speex C structs
extern "C" {
struct SpeexEchoState_;
typedef struct SpeexEchoState_ SpeexEchoState;
struct SpeexPreprocessState_;
typedef struct SpeexPreprocessState_ SpeexPreprocessState;
}

namespace jami {

class SpeexAudioProcessor final : public AudioProcessor
{
public:
    SpeexAudioProcessor(AudioFormat format, unsigned frameSize);
    ~SpeexAudioProcessor() = default;

    std::shared_ptr<AudioFrame> getProcessed() override;

    void enableEchoCancel(bool enabled) override;
    void enableNoiseSuppression(bool enabled) override;
    void enableAutomaticGainControl(bool enabled) override;

private:
    using SpeexEchoStatePtr = std::unique_ptr<SpeexEchoState, void (*)(SpeexEchoState*)>;
    using SpeexPreprocessStatePtr
        = std::unique_ptr<SpeexPreprocessState, void (*)(SpeexPreprocessState*)>;

    // multichannel, one for the entire audio processor
    SpeexEchoStatePtr echoState;

    // one for each channel
    std::vector<SpeexPreprocessStatePtr> preprocessorStates;

    AudioBuffer iProcBuffer;

    // if we should do echo cancellation
    bool shouldAEC {false};
};
} // namespace jami
