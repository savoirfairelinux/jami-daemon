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

#include <webrtc/modules/audio_processing/include/audio_processing.h>

namespace jami {

WebRTCAudioProcessor::WebRTCAudioProcessor(AudioFormat format, unsigned frameSize)
    : AudioProcessor(format, frameSize)
    , fRecordBuffer_(format.nb_channels, std::vector<float>(frameSize_, 0))
    , fPlaybackBuffer_(format.nb_channels, std::vector<float>(frameSize_, 0))
    , iRecordBuffer_(frameSize_, format)
    , iPlaybackBuffer_(frameSize_, format)
{
    webrtc::Config config;
    config.Set<webrtc::ExtendedFilter>(new webrtc::ExtendedFilter(true));
    config.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(true));

    apm.reset(webrtc::AudioProcessing::Create(config));

    webrtc::StreamConfig streamConfig(format.sample_rate, format.nb_channels);
    webrtc::ProcessingConfig pconfig = {
        streamConfig, /* input stream */
        streamConfig, /* output stream */
        streamConfig, /* reverse input stream */
        streamConfig, /* reverse output stream */
    };

    if (apm->Initialize(pconfig) != webrtc::AudioProcessing::kNoError) {
        JAMI_ERR("[webrtc-ap] Error initialising audio processing module");
    }

    // vad
    apm->voice_detection()->Enable(true);

    // aec
    apm->echo_cancellation()->set_suppression_level(
        webrtc::EchoCancellation::SuppressionLevel::kModerateSuppression);
    apm->echo_cancellation()->enable_drift_compensation(true);
    apm->echo_cancellation()->Enable(true);

    // hpf
    apm->high_pass_filter()->Enable(true);

    // ns
    apm->noise_suppression()->set_level(webrtc::NoiseSuppression::kHigh);
    apm->noise_suppression()->Enable(true);

    // agc
    apm->gain_control()->set_analog_level_limits(0, 255);
    apm->gain_control()->set_mode(webrtc::GainControl::kAdaptiveAnalog);
    apm->gain_control()->Enable(true);
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
    if (!playback || !record)
        return {};

    auto processed = std::make_shared<AudioFrame>(format_, frameSize_);

    //webrtc::StreamConfig& sc = streamConfig;
    webrtc::StreamConfig sc(format_.sample_rate, format_.nb_channels);

    // analyze deinterleaved float playback data
    iPlaybackBuffer_.deinterleave((const AudioSample*) playback->pointer()->data[0],
                                  frameSize_,
                                  format_.nb_channels);
    std::vector<float*> playData {format_.nb_channels};
    for (unsigned c = 0; c < format_.nb_channels; ++c) {
        playData[c] = fPlaybackBuffer_[c].data();
        iPlaybackBuffer_.channelToFloat(playData[c], c);
    }
    if (apm->ProcessReverseStream(playData.data(), sc, sc, playData.data())
        != webrtc::AudioProcessing::kNoError)
        JAMI_ERR("[webrtc-ap] ProcessReverseStream failed");

    // process deinterleaved float recorded data
    iRecordBuffer_.deinterleave((const AudioSample*) record->pointer()->data[0],
                                frameSize_,
                                format_.nb_channels);
    std::vector<float*> recData {format_.nb_channels};
    for (unsigned c = 0; c < format_.nb_channels; ++c) {
        recData[c] = fRecordBuffer_[c].data();
        iRecordBuffer_.channelToFloat(recData[c], c);
    }
    // TODO: implement this correctly (it MUST be called prior to ProcessStream)
    // delay = (t_render - t_analyze) + (t_process - t_capture)
    apm->set_stream_delay_ms(0);
    apm->gain_control()->set_stream_analog_level(analogLevel_);
    apm->echo_cancellation()->set_stream_drift_samples(driftSamples);
    if (apm->ProcessStream(recData.data(), sc, sc, recData.data())
        != webrtc::AudioProcessing::kNoError)
        JAMI_ERR("[webrtc-ap] ProcessStream failed");
    analogLevel_ = apm->gain_control()->stream_analog_level();

    // return interleaved s16 data
    iRecordBuffer_.convertFloatPlanarToSigned16((uint8_t**) recData.data(),
                                                frameSize_,
                                                format_.nb_channels);
    iRecordBuffer_.interleave((AudioSample*) processed->pointer()->data[0]);

    processed->has_voice = apm->voice_detection()->stream_has_voice();
    return processed;
}

} // namespace jami
