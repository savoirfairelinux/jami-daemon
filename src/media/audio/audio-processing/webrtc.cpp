/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#if __has_include(<common_audio/vad/include/webrtc_vad.h>)
#include <common_audio/vad/include/webrtc_vad.h>
#define HAVE_WEBRTC_VAD 1
#else
// Only installed by the Jami contrib build (see contrib/src/webrtc-audio-processing)
#define HAVE_WEBRTC_VAD 0
#endif

#include <algorithm>
#include <cmath>

namespace jami {

inline size_t
webrtcFrameSize(AudioFormat format)
{
    return (size_t) (webrtc::AudioProcessing::kChunkSizeMs * format.sample_rate / 1000);
}

constexpr int webrtcNoError = webrtc::AudioProcessing::kNoError;

void
WebRTCAudioProcessor::VadDeleter::operator()(VadInst* vad) const
{
#if HAVE_WEBRTC_VAD
    WebRtcVad_Free(vad);
#else
    (void) vad;
#endif
}

WebRTCAudioProcessor::WebRTCAudioProcessor(AudioFormat format, unsigned /* frameSize */)
    : AudioProcessor(format.withSampleFormat(AV_SAMPLE_FMT_FLTP), webrtcFrameSize(format))
{
    JAMI_LOG("[webrtc-ap] WebRTCAudioProcessor, frame size = {:d} (={:d} ms), channels = {:d}",
             frameSize_,
             frameDurationMs_,
             format_.nb_channels);

    apm = webrtc::AudioProcessingBuilder().Create();

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

    // The APM dropped its voice detector in 1.0; use the standalone WebRTC VAD
    // (what the old VoiceDetection component wrapped) on the first channel.
#if HAVE_WEBRTC_VAD
    if (WebRtcVad_ValidRateAndFrameLength((int) format_.sample_rate, frameSize_) == 0) {
        vad_.reset(WebRtcVad_Create());
        if (WebRtcVad_Init(vad_.get()) != 0) {
            JAMI_ERROR("[webrtc-ap] Error initialising voice activity detection");
            vad_.reset();
        }
        vadBuffer_.resize(frameSize_);
    } else {
        JAMI_WARNING("[webrtc-ap] {:d} Hz unsupported by the WebRTC VAD, voice activity disabled", format_.sample_rate);
    }
#else
    JAMI_WARNING("[webrtc-ap] Built without the WebRTC VAD, voice activity disabled");
#endif
}

void
WebRTCAudioProcessor::applyConfig()
{
    apm->ApplyConfig(config_);
}

void
WebRTCAudioProcessor::enableNoiseSuppression(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableNoiseSuppression {}", enabled);
    config_.noise_suppression.enabled = enabled;
    config_.noise_suppression.level = webrtc::AudioProcessing::Config::NoiseSuppression::kVeryHigh;
    config_.high_pass_filter.enabled = enabled;
    applyConfig();
}

void
WebRTCAudioProcessor::enableAutomaticGainControl(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableAutomaticGainControl {}", enabled);
    config_.gain_controller1.enabled = enabled;
    config_.gain_controller1.mode = webrtc::AudioProcessing::Config::GainController1::kAdaptiveAnalog;
    applyConfig();
}

void
WebRTCAudioProcessor::enableEchoCancel(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableEchoCancel {}", enabled);
    config_.echo_canceller.enabled = enabled;
    config_.echo_canceller.mobile_mode = false;
    applyConfig();
}

void
WebRTCAudioProcessor::enableVoiceActivityDetection(bool enabled)
{
    JAMI_LOG("[webrtc-ap] enableVoiceActivityDetection {}", enabled);
    if (!vad_) {
        if (enabled)
            JAMI_ERROR("[webrtc-ap] Voice activity detection unavailable");
        return;
    }
#if HAVE_WEBRTC_VAD
    // Mode 3 is what VoiceDetection::kVeryLowLikelihood mapped to.
    if (WebRtcVad_set_mode(vad_.get(), enabled ? 3 : 0) != 0) {
        JAMI_ERROR("[webrtc-ap] Error setting voice detection mode");
    }
#endif
    detectVoice_ = enabled;
}

std::shared_ptr<AudioFrame>
WebRTCAudioProcessor::getProcessed()
{
    if (tidyQueues()) {
        return {};
    }

    auto playback = playbackQueue_.dequeue();
    auto record = recordQueue_.dequeue();
    if (!playback || !record) {
        return {};
    }
    webrtc::StreamConfig sc((int) format_.sample_rate, (int) format_.nb_channels);

    // process reverse in place
    float** playData = (float**) playback->pointer()->extended_data;
    if (apm->ProcessReverseStream(playData, sc, sc, playData) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] ProcessReverseStream failed");
    }

    // AEC3 estimates the delay itself; 0 keeps the API contract satisfied.
    if (apm->set_stream_delay_ms(0) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] set_stream_delay_ms failed");
    }
    if (config_.gain_controller1.enabled) {
        apm->set_stream_analog_level(analogLevel_);
    }

    // process deinterleaved float recorded data in place
    float** recData = (float**) record->pointer()->extended_data;
    if (apm->ProcessStream(recData, sc, sc, recData) != webrtcNoError) {
        JAMI_ERROR("[webrtc-ap] ProcessStream failed");
    }

    if (config_.gain_controller1.enabled) {
        analogLevel_ = apm->recommended_stream_analog_level();
    }

    bool hasVoice = false;
#if HAVE_WEBRTC_VAD
    if (detectVoice_ && vad_) {
        std::transform(recData[0], recData[0] + frameSize_, vadBuffer_.begin(), [](float s) {
            return (int16_t) std::lround(std::clamp(s, -1.f, 1.f) * 32767.f);
        });
        hasVoice = WebRtcVad_Process(vad_.get(), (int) format_.sample_rate, vadBuffer_.data(), vadBuffer_.size()) == 1;
    }
#endif
    record->has_voice = detectVoice_ && getStabilizedVoiceActivity(hasVoice);
    return record;
}

} // namespace jami
