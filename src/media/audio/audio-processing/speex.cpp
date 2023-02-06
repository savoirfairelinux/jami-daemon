/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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

#include "speex.h"

#include "audio/audiolayer.h"

#ifndef _MSC_VER
#include <speex/speex_config_types.h>
#endif
extern "C" {
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
}

#include <cstdint>
#include <memory>
#include <vector>

namespace jami {

SpeexAudioProcessor::SpeexAudioProcessor(AudioFormat format, unsigned frameSize)
    : AudioProcessor(format, frameSize)
    , echoState(speex_echo_state_init_mc((int) frameSize,
                                         (int) frameSize * 16,
                                         (int) format.nb_channels,
                                         (int) format.nb_channels),
                &speex_echo_state_destroy)
    , iProcBuffer(frameSize_, format)
{
    JAMI_DBG("[speex-dsp] SpeexAudioProcessor, frame size = %d (=%d ms), channels = %d",
             frameSize,
             frameDurationMs_,
             format.nb_channels);
    // set up speex echo state
    speex_echo_ctl(echoState.get(), SPEEX_ECHO_SET_SAMPLING_RATE, &format_.sample_rate);

    // speex specific value to turn feature on (need to pass a pointer to it)
    spx_int32_t speexOn = 1;

    // probability integers, i.e. 50 means 50%
    // vad will be true if speex's raw probability calculation is higher than this in any case
    spx_int32_t probStart = 99;

    // vad will be true if voice was active last frame
    //     AND speex's raw probability calculation is higher than this
    spx_int32_t probContinue = 90;

    // maximum noise suppression in dB (negative)
    spx_int32_t maxNoiseSuppress = -50;

    // set up speex preprocess states, one for each channel
    // note that they are not enabled here, but rather in the enable* functions
    for (unsigned int i = 0; i < format.nb_channels; i++) {
        auto channelPreprocessorState
            = SpeexPreprocessStatePtr(speex_preprocess_state_init((int) frameSize,
                                                                  (int) format.sample_rate),
                                      &speex_preprocess_state_destroy);

        // set max noise suppression level
        speex_preprocess_ctl(channelPreprocessorState.get(),
                             SPEEX_PREPROCESS_SET_NOISE_SUPPRESS,
                             &maxNoiseSuppress);

        // set up voice activity values
        speex_preprocess_ctl(channelPreprocessorState.get(), SPEEX_PREPROCESS_SET_VAD, &speexOn);
        speex_preprocess_ctl(channelPreprocessorState.get(),
                             SPEEX_PREPROCESS_SET_PROB_START,
                             &probStart);
        speex_preprocess_ctl(channelPreprocessorState.get(),
                             SPEEX_PREPROCESS_SET_PROB_CONTINUE,
                             &probContinue);

        // keep track of this channel's preprocessor state
        preprocessorStates.push_back(std::move(channelPreprocessorState));
    }

    JAMI_INFO("[speex-dsp] Done initializing");
}

void
SpeexAudioProcessor::enableEchoCancel(bool enabled)
{
    JAMI_DBG("[speex-dsp] enableEchoCancel %d", enabled);
    // need to set member variable so we know to do it in getProcessed
    shouldAEC = enabled;

    if (enabled) {
        // reset the echo canceller
        speex_echo_state_reset(echoState.get());

        for (auto& channelPreprocessorState : preprocessorStates) {
            // attach our already-created echo canceller
            speex_preprocess_ctl(channelPreprocessorState.get(),
                                 SPEEX_PREPROCESS_SET_ECHO_STATE,
                                 echoState.get());
        }
    } else {
        for (auto& channelPreprocessorState : preprocessorStates) {
            // detach echo canceller (set it to NULL)
            // don't destroy it though, we will reset it when necessary
            speex_preprocess_ctl(channelPreprocessorState.get(),
                                 SPEEX_PREPROCESS_SET_ECHO_STATE,
                                 NULL);
        }
    }
}

void
SpeexAudioProcessor::enableNoiseSuppression(bool enabled)
{
    JAMI_DBG("[speex-dsp] enableNoiseSuppression %d", enabled);
    spx_int32_t speexSetValue = (spx_int32_t) enabled;

    // for each preprocessor
    for (auto& channelPreprocessorState : preprocessorStates) {
        // set denoise status
        speex_preprocess_ctl(channelPreprocessorState.get(),
                             SPEEX_PREPROCESS_SET_DENOISE,
                             &speexSetValue);
        // set de-reverb status
        speex_preprocess_ctl(channelPreprocessorState.get(),
                             SPEEX_PREPROCESS_SET_DEREVERB,
                             &speexSetValue);
    }
}

void
SpeexAudioProcessor::enableAutomaticGainControl(bool enabled)
{
    JAMI_DBG("[speex-dsp] enableAutomaticGainControl %d", enabled);
    spx_int32_t speexSetValue = (spx_int32_t) enabled;

    // for each preprocessor
    for (auto& channelPreprocessorState : preprocessorStates) {
        // set AGC status
        speex_preprocess_ctl(channelPreprocessorState.get(),
                             SPEEX_PREPROCESS_SET_AGC,
                             &speexSetValue);
    }
}

void
SpeexAudioProcessor::enableVoiceActivityDetection(bool enabled)
{
    JAMI_DBG("[speex-dsp] enableVoiceActivityDetection %d", enabled);

    shouldDetectVoice = enabled;

    spx_int32_t speexSetValue = (spx_int32_t) enabled;
    for (auto& channelPreprocessorState : preprocessorStates) {
        speex_preprocess_ctl(channelPreprocessorState.get(),
                             SPEEX_PREPROCESS_SET_VAD,
                             &speexSetValue);
    }
}

std::shared_ptr<AudioFrame>
SpeexAudioProcessor::getProcessed()
{
    if (tidyQueues()) {
        return {};
    }

    auto playback = playbackQueue_.dequeue();
    auto record = recordQueue_.dequeue();

    if (!playback || !record) {
        return {};
    }

    auto processed = std::make_shared<AudioFrame>(record->getFormat(), record->getFrameSize());

    if (shouldAEC) {
        // we want to echo cancel
        // multichannel, output into processed
        speex_echo_cancellation(echoState.get(),
                                (int16_t*) record->pointer()->data[0],
                                (int16_t*) playback->pointer()->data[0],
                                (int16_t*) processed->pointer()->data[0]);
    } else {
        // don't want to echo cancel, so just use record frame instead
        processed = record;
    }

    // deinterleave processed into channels
    std::vector<int16_t*> procData {format_.nb_channels};
    iProcBuffer.deinterleave((const AudioSample*) processed->pointer()->data[0],
                             frameSize_,
                             format_.nb_channels);

    // point procData to correct channels
    for (unsigned int channel = 0; channel < format_.nb_channels; channel++) {
        procData[channel] = iProcBuffer.getChannel(channel)->data();
    }

    // overall voice activity
    bool overallVad = false;
    // current channel voice activity
    int channelVad;

    // run preprocess on each channel
    int channel = 0;
    for (auto& channelPreprocessorState : preprocessorStates) {
        // preprocesses in place, returns voice activity boolean
        channelVad = speex_preprocess_run(channelPreprocessorState.get(), procData[channel]);

        // boolean OR
        overallVad |= channelVad;

        channel += 1;
    }

    // reinterleave into processed
    iProcBuffer.interleave((AudioSample*) processed->pointer()->data[0]);

    // add stabilized voice activity to the AudioFrame
    processed->has_voice = shouldDetectVoice && getStabilizedVoiceActivity(overallVad);

    return processed;
}

} // namespace jami
