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

#include "webrtc.h"

#include <webrtc/modules/audio_processing/include/audio_processing.h>

namespace jami {

constexpr int webrtcNoError = webrtc::AudioProcessing::kNoError;

WebRTCAudioProcessor::WebRTCAudioProcessor(AudioFormat format, unsigned frameSize)
    : AudioProcessor(format, frameSize)
    , fRecordBuffer_(format.nb_channels, std::vector<float>(frameSize_, 0))
    , fPlaybackBuffer_(format.nb_channels, std::vector<float>(frameSize_, 0))
    , iRecordBuffer_(frameSize_, format)
    , iPlaybackBuffer_(frameSize_, format)
{
    JAMI_DBG("[webrtc-ap] WebRTCAudioProcessor, frame size = %d (=%d ms), channels = %d",
             frameSize,
             frameDurationMs_,
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

    if (apm->Initialize(pconfig) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error initialising audio processing module");
    }

    JAMI_INFO("[webrtc-ap] Done initializing");
}

void
WebRTCAudioProcessor::enableNoiseSuppression(bool enabled)
{
    JAMI_DBG("[webrtc-ap] enableNoiseSuppression %d", enabled);
    if (apm->noise_suppression()->Enable(enabled) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error enabling noise suppression");
    }
    if (apm->noise_suppression()->set_level(webrtc::NoiseSuppression::kVeryHigh) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error setting noise suppression level");
    }
    if (apm->high_pass_filter()->Enable(enabled) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error enabling high pass filter");
    }
}

void
WebRTCAudioProcessor::enableAutomaticGainControl(bool enabled)
{
    JAMI_DBG("[webrtc-ap] enableAutomaticGainControl %d", enabled);
    if (apm->gain_control()->Enable(enabled) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error enabling automatic gain control");
    }
    if (apm->gain_control()->set_analog_level_limits(0, 255) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error setting automatic gain control analog level limits");
    }
    if (apm->gain_control()->set_mode(webrtc::GainControl::kAdaptiveAnalog) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error setting automatic gain control mode");
    }
}

void
WebRTCAudioProcessor::enableEchoCancel(bool enabled)
{
    JAMI_DBG("[webrtc-ap] enableEchoCancel %d", enabled);

    if (apm->echo_cancellation()->Enable(enabled) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error enabling echo cancellation");
    }
    if (apm->echo_cancellation()->set_suppression_level(
            webrtc::EchoCancellation::SuppressionLevel::kHighSuppression)
        != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error setting echo cancellation level");
    }
    if (apm->echo_cancellation()->enable_drift_compensation(true) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error enabling echo cancellation drift compensation");
    }
}

void
WebRTCAudioProcessor::enableVoiceActivityDetection(bool enabled)
{
    JAMI_DBG("[webrtc-ap] enableVoiceActivityDetection %d", enabled);
    if (apm->voice_detection()->Enable(enabled) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error enabling voice activation detection");
    }
    if (apm->voice_detection()->set_likelihood(webrtc::VoiceDetection::kVeryLowLikelihood)
        != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error setting voice detection likelihood");
    }
    // asserted to be 10 in voice_detection_impl.cc
    if (apm->voice_detection()->set_frame_size_ms(10) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] Error setting voice detection frame size");
    }
}

std::shared_ptr<AudioFrame>
WebRTCAudioProcessor::getProcessed()
{
    if (tidyQueues()) {
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
    if (apm->ProcessReverseStream(playData.data(), sc, sc, playData.data()) != webrtcNoError) {
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
    if (apm->set_stream_delay_ms(0) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] set_stream_delay_ms failed");
    }

    if (apm->gain_control()->set_stream_analog_level(analogLevel_) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] set_stream_analog_level failed");
    }
    apm->echo_cancellation()->set_stream_drift_samples(driftSamples);

    // process in place
    if (apm->ProcessStream(recData.data(), sc, sc, recData.data()) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] ProcessStream failed");
    }

    analogLevel_ = apm->gain_control()->stream_analog_level();

    // return interleaved s16 data
    iRecordBuffer_.convertFloatPlanarToSigned16((uint8_t**) recData.data(),
                                                frameSize_,
                                                format_.nb_channels);
    iRecordBuffer_.interleave((AudioSample*) processed->pointer()->data[0]);

    processed->has_voice = apm->voice_detection()->is_enabled()
                           && getStabilizedVoiceActivity(apm->voice_detection()->stream_has_voice());

    return processed;
}

} // namespace jami
