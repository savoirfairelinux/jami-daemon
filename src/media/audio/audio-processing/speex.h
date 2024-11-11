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
    void enableVoiceActivityDetection(bool enabled) override;

private:
    using SpeexEchoStatePtr = std::unique_ptr<SpeexEchoState, void (*)(SpeexEchoState*)>;
    using SpeexPreprocessStatePtr
        = std::unique_ptr<SpeexPreprocessState, void (*)(SpeexPreprocessState*)>;

    // multichannel, one for the entire audio processor
    SpeexEchoStatePtr echoState;

    // one for each channel
    std::vector<SpeexPreprocessStatePtr> preprocessorStates;

    std::unique_ptr<AudioFrame> procBuffer {};
    Resampler deinterleaveResampler;
    Resampler interleaveResampler;

    // if we should do echo cancellation
    bool shouldAEC {false};

    // if we should do voice activity detection
    // preprocess_run returns 1 if vad is disabled, so we have to know whether or not to ignore it
    bool shouldDetectVoice {false};
};
} // namespace jami
