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

#include "webrtc.h"

#include <chrono>
#include <cstdint>
#include <ratio>
#include <webrtc/modules/audio_processing/include/audio_processing.h>

namespace jami {

WebRTCAudioProcessor::WebRTCAudioProcessor(AudioFormat format, unsigned frameSize)
    : AudioProcessor(format, frameSize)
    , fRecordBuffer_(format.nb_channels, std::vector<float>(frameSize_, 0))
    , fPlaybackBuffer_(format.nb_channels, std::vector<float>(frameSize_, 0))
    , iRecordBuffer_(frameSize_, format)
    , iPlaybackBuffer_(frameSize_, format)
{
    JAMI_DBG("[webrtc-ap] WebRTCAudioProcessor, frame size = %d (=~%d ms), channels = %d",
             frameSize,
             (int) (frameSize * (1.0 / format.sample_rate) * 1000),
             format.nb_channels);
    webrtc::Config config;
    config.Set<webrtc::ExtendedFilter>(new webrtc::ExtendedFilter(true));
    config.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(true));

    apm.reset(webrtc::AudioProcessing::Create(config));

    webrtc::StreamConfig streamConfig((int) format.sample_rate, (int) format.nb_channels);
    webrtc::ProcessingConfig pconfig = {
        streamConfig, /* input stream */
        streamConfig, /* output stream */
        streamConfig, /* reverse input stream */
        streamConfig, /* reverse output stream */
    };

    if (apm->Initialize(pconfig) != webrtc::AudioProcessing::kNoError) {
        JAMI_ERR("[webrtc-ap] Error initialising audio processing module");
    }

    // voice activity
    apm->voice_detection()->Enable(true);
    // TODO: change likelihood?
    apm->voice_detection()->set_likelihood(webrtc::VoiceDetection::kLowLikelihood);
    // TODO: change? asserted to be 10 in voice_detection_impl.cc???
    apm->voice_detection()->set_frame_size_ms(10);

    JAMI_INFO("[webrtc-ap] Done initializing");
}

void
WebRTCAudioProcessor::enableNoiseSuppression(bool enabled)
{
    JAMI_DBG("[webrtc-ap] enableNoiseSuppression %d", enabled);
    apm->noise_suppression()->Enable(enabled);
    apm->noise_suppression()->set_level(webrtc::NoiseSuppression::kVeryHigh);

    apm->high_pass_filter()->Enable(enabled);
}

void
WebRTCAudioProcessor::enableAutomaticGainControl(bool enabled)
{
    JAMI_DBG("[webrtc-ap] enableAutomaticGainControl %d", enabled);
    apm->gain_control()->Enable(enabled);
    apm->gain_control()->set_analog_level_limits(0, 255);
    apm->gain_control()->set_mode(webrtc::GainControl::kAdaptiveAnalog);
}

void
WebRTCAudioProcessor::enableEchoCancel(bool enabled)
{
    JAMI_DBG("[webrtc-ap] enableEchoCancel %d", enabled);

    apm->echo_cancellation()->Enable(enabled);
    apm->echo_cancellation()->set_suppression_level(
        webrtc::EchoCancellation::SuppressionLevel::kHighSuppression);
    apm->echo_cancellation()->enable_drift_compensation(true);
}

std::shared_ptr<AudioFrame>
WebRTCAudioProcessor::getProcessed()
{
    while (recordQueue_.samples() > recordQueue_.frameSize() * 10) {
        JAMI_DBG("record overflow %d / %d", recordQueue_.samples(), frameSize_);
        recordQueue_.dequeue();
    }
    while (playbackQueue_.samples() > playbackQueue_.frameSize() * 10) {
        JAMI_DBG("playback overflow %d / %d", playbackQueue_.samples(), frameSize_);
        playbackQueue_.dequeue();
    }
    if (recordQueue_.samples() < recordQueue_.frameSize()
        || playbackQueue_.samples() < playbackQueue_.frameSize()) {
        // If there are not enough samples in either queue, we can't
        // process anything.
        // JAMI_DBG("underrun p:%d / r:%d", playbackQueue_.samples(), recordQueue_.samples());
        return {};
    }

    int driftSamples = playbackQueue_.samples() - recordQueue_.samples();

    auto playback = playbackQueue_.dequeue();
    auto record = recordQueue_.dequeue();
    if (!playback || !record) {
        return {};
    }

    auto processed = std::make_shared<AudioFrame>(format_, frameSize_);

    // webrtc::StreamConfig& sc = streamConfig;
    webrtc::StreamConfig sc((int) format_.sample_rate, (int) format_.nb_channels);

    // analyze deinterleaved float playback data
    iPlaybackBuffer_.deinterleave((const AudioSample*) playback->pointer()->data[0],
                                  frameSize_,
                                  format_.nb_channels);
    std::vector<float*> playData {format_.nb_channels};
    for (unsigned channel = 0; channel < format_.nb_channels; ++channel) {
        // point playData channel to appropriate data location
        playData[channel] = fPlaybackBuffer_[channel].data();

        // write playback to playData channel
        iPlaybackBuffer_.channelToFloat(playData[channel], (int) channel);
    }

    // process reverse in place
    if (apm->ProcessReverseStream(playData.data(), sc, sc, playData.data())
        != webrtc::AudioProcessing::kNoError) {
        JAMI_ERR("[webrtc-ap] ProcessReverseStream failed");
    }

    // process deinterleaved float recorded data
    iRecordBuffer_.deinterleave((const AudioSample*) record->pointer()->data[0],
                                frameSize_,
                                format_.nb_channels);
    std::vector<float*> recData {format_.nb_channels};
    for (unsigned int channel = 0; channel < format_.nb_channels; ++channel) {
        // point recData channel to appropriate data location
        recData[channel] = fRecordBuffer_[channel].data();

        // write data to recData channel
        iRecordBuffer_.channelToFloat(recData[channel], (int) channel);
    }
    // TODO: maybe implement this to see if it's better than automatic drift compensation
    // (it MUST be called prior to ProcessStream)
    // delay = (t_render - t_analyze) + (t_process - t_capture)
    apm->set_stream_delay_ms(0);

    apm->gain_control()->set_stream_analog_level(analogLevel_);
    apm->echo_cancellation()->set_stream_drift_samples(driftSamples);

    // process in place
    if (apm->ProcessStream(recData.data(), sc, sc, recData.data())
        != webrtc::AudioProcessing::kNoError) {
        JAMI_ERR("[webrtc-ap] ProcessStream failed");
    }

    analogLevel_ = apm->gain_control()->stream_analog_level();

    // return interleaved s16 data
    iRecordBuffer_.convertFloatPlanarToSigned16((uint8_t**) recData.data(),
                                                frameSize_,
                                                format_.nb_channels);
    iRecordBuffer_.interleave((AudioSample*) processed->pointer()->data[0]);

    // add the voice activity to the returned AudioFrame
    processed->has_voice = apm->voice_detection()->stream_has_voice();

    if (lastVoiceState != processed->has_voice) {
        // JAMI_INFO("voice now %s", processed->has_voice ? "active" : "inactive");
        lastVoiceState = processed->has_voice;
    }

    return processed;
}

} // namespace jami
