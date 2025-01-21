/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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

#include "webrtc.h"
#include "logger.h"

#include <modules/audio_processing/include/audio_processing.h>

namespace jami {

struct ConfigImpl {
    webrtc::AudioProcessing::Config c;
};

inline size_t
webrtcFrameSize(AudioFormat format)
{
    return (size_t) (webrtc::AudioProcessing::kChunkSizeMs * format.sample_rate / 1000);
}

constexpr int webrtcNoError = webrtc::AudioProcessing::kNoError;

WebRTCAudioProcessor::WebRTCAudioProcessor(AudioFormat format, unsigned /* frameSize */)
    : AudioProcessor(format.withSampleFormat(AV_SAMPLE_FMT_FLTP), webrtcFrameSize(format))
    , apm(nullptr, [](webrtc::AudioProcessing* ptr) { if (ptr) ptr->Release(); })
{
    JAMI_LOG("[webrtc-ap] WebRTCAudioProcessor, frame size = {:d} (={:d} ms), channels = {:d}",
             frameSize_,
             frameDurationMs_,
             format_.nb_channels);
    config.reset(new ConfigImpl());
    config->c.echo_canceller.enabled = true;
    config->c.echo_canceller.mobile_mode = false;
    config->c.high_pass_filter.enabled = true;
    config->c.noise_suppression.enabled = true;
    config->c.noise_suppression.level = webrtc::AudioProcessing::Config::NoiseSuppression::kHigh;
    config->c.noise_suppression.analyze_linear_aec_output_when_available = true;

    apm.reset(webrtc::AudioProcessingBuilder().Create().release());
    apm->ApplyConfig(config->c);
}

void
WebRTCAudioProcessor::enableNoiseSuppression(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableNoiseSuppression {}", enabled);
    config->c.high_pass_filter.enabled = enabled;
    config->c.noise_suppression.enabled = enabled;
    apm->ApplyConfig(config->c);
}

void
WebRTCAudioProcessor::enableAutomaticGainControl(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableAutomaticGainControl {}", enabled);
    config->c.capture_level_adjustment.enabled = enabled;
    config->c.gain_controller2.enabled = enabled;
    apm->ApplyConfig(config->c);
}

void
WebRTCAudioProcessor::enableEchoCancel(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableEchoCancel {}", enabled);
    config->c.echo_canceller.enabled = enabled;
    config->c.echo_canceller.export_linear_aec_output = enabled;
    apm->ApplyConfig(config->c);
}

void
WebRTCAudioProcessor::enableVoiceActivityDetection(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableVoiceActivityDetection {}", enabled);
    /*config->c.voice_detection.enabled = enabled;
    apm->ApplyConfig(config->c);*/
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

    float** recData = (float**) record->pointer()->extended_data;
    if (apm->ProcessStream(recData, sc, sc, recData) != webrtcNoError) {
        JAMI_ERR("[webrtc-ap] ProcessStream failed");
    }

    if (auto voiceInfo = apm->GetStatistics().voice_detected) {
        record->has_voice = voiceInfo.value();
    } else {
        record->has_voice = false;
    }
    return record;
}

} // namespace jami
