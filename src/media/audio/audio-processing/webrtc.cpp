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
#include "logger.h"

#include <webrtc/modules/audio_processing/include/audio_processing.h>

namespace jami {

inline size_t
webrtcFrameSize(AudioFormat format)
{
    return (size_t) (webrtc::AudioProcessing::kChunkSizeMs * format.sample_rate / 1000);
}

constexpr int webrtcNoError = webrtc::AudioProcessing::kNoError;

WebRTCAudioProcessor::WebRTCAudioProcessor(AudioFormat format, unsigned /* frameSize */)
    : AudioProcessor(format.withSampleFormat(AV_SAMPLE_FMT_FLTP), webrtcFrameSize(format))
{
    JAMI_LOG("[webrtc-ap] WebRTCAudioProcessor, frame size = {:d} (={:d} ms), channels = {:d}",
             frameSize_,
             frameDurationMs_,
             format_.nb_channels);
    webrtc::Config config;
    config.Set<webrtc::ExtendedFilter>(new webrtc::ExtendedFilter(true));
    config.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(true));

    apm.reset(webrtc::AudioProcessing::Create(config));

    webrtc::StreamConfig streamConfig((int) format_.sample_rate, (int) format_.nb_channels);
    webrtc::ProcessingConfig pconfig = {
        streamConfig, /* input stream */
        streamConfig, /* output stream */
        streamConfig, /* reverse input stream */
        streamConfig, /* reverse output stream */
    };

    if (apm->Initialize(pconfig) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error initialising audio processing module");
    }
}

void
WebRTCAudioProcessor::enableNoiseSuppression(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableNoiseSuppression {}", enabled);
    if (apm->noise_suppression()->Enable(enabled) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error enabling noise suppression");
    }
    if (apm->noise_suppression()->set_level(webrtc::NoiseSuppression::kVeryHigh) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error setting noise suppression level");
    }
    if (apm->high_pass_filter()->Enable(enabled) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error enabling high pass filter");
    }
}

void
WebRTCAudioProcessor::enableAutomaticGainControl(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableAutomaticGainControl {}", enabled);
    if (apm->gain_control()->Enable(enabled) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error enabling automatic gain control");
    }
    if (apm->gain_control()->set_analog_level_limits(0, 255) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error setting automatic gain control analog level limits");
    }
    if (apm->gain_control()->set_mode(webrtc::GainControl::kAdaptiveAnalog) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error setting automatic gain control mode");
    }
}

void
WebRTCAudioProcessor::enableEchoCancel(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableEchoCancel {}", enabled);

    if (apm->echo_cancellation()->Enable(enabled) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error enabling echo cancellation");
    }
    if (apm->echo_cancellation()->set_suppression_level(
            webrtc::EchoCancellation::SuppressionLevel::kHighSuppression)
        != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error setting echo cancellation level");
    }
    if (apm->echo_cancellation()->enable_drift_compensation(true) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error enabling echo cancellation drift compensation");
    }
}

void
WebRTCAudioProcessor::enableVoiceActivityDetection(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableVoiceActivityDetection {}", enabled);
    if (apm->voice_detection()->Enable(enabled) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error enabling voice activation detection");
    }
    if (apm->voice_detection()->set_likelihood(webrtc::VoiceDetection::kVeryLowLikelihood)
        != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error setting voice detection likelihood");
    }
    // asserted to be 10 in voice_detection_impl.cc
    if (apm->voice_detection()->set_frame_size_ms(10) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] Error setting voice detection frame size");
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
    webrtc::StreamConfig sc((int) format_.sample_rate, (int) format_.nb_channels);

    // process reverse in place
    float** playData = (float**) playback->pointer()->extended_data;
    if (apm->ProcessReverseStream(playData, sc, sc, playData) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] ProcessReverseStream failed");
    }

    // process deinterleaved float recorded data
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
    float** recData = (float**) record->pointer()->extended_data;
    if (apm->ProcessStream(recData, sc, sc, recData) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] ProcessStream failed");
    }

    analogLevel_ = apm->gain_control()->stream_analog_level();
    record->has_voice = apm->voice_detection()->is_enabled()
                           && getStabilizedVoiceActivity(apm->voice_detection()->stream_has_voice());
    return record;
}

} // namespace jami
