/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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

#include "webrtc_echo_canceller.h"

#include <webrtc/modules/audio_processing/include/audio_processing.h>
#include <webrtc/modules/interface/module_common_types.h>

namespace jami {

WebRTCEchoCanceller::WebRTCEchoCanceller(AudioFormat format, unsigned frameSize)
    : EchoCanceller(format, frameSize)
    , pimpl_(std::make_unique<WebRTCAPMImpl>(format, frameSize))
{}

struct WebRTCEchoCanceller::WebRTCAPMImpl
{
    using APMPtr = std::unique_ptr<webrtc::AudioProcessing>;
    APMPtr apm;

    WebRTCAPMImpl(AudioFormat format, unsigned frameSize)
    {
        webrtc::ProcessingConfig pconfig;
        webrtc::Config config;

        config.Set<webrtc::ExtendedFilter>(new webrtc::ExtendedFilter(true));
        config.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(true));

        apm.reset(webrtc::AudioProcessing::Create(config));

        webrtc::StreamConfig sc(format.sample_rate, format.nb_channels);

        pconfig = {
            sc, /* input stream */
            sc, /* output stream */
            sc, /* reverse input stream */
            sc, /* reverse output stream */
        };

        if (apm->Initialize(pconfig) != webrtc::AudioProcessing::kNoError) {
            JAMI_ERR("Error initialising audio processing module");
        }

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
    }
};

void
WebRTCEchoCanceller::putRecorded(std::shared_ptr<AudioFrame>&& buf)
{
    playbackQueue_.enqueue(std::move(buf));
}

void
WebRTCEchoCanceller::putPlayback(const std::shared_ptr<AudioFrame>& buf)
{
    auto c = buf;
    recordQueue_.enqueue(std::move(c));
}

std::shared_ptr<AudioFrame>
WebRTCEchoCanceller::getProcessed()
{
    if (playbackQueue_.samples() < playbackQueue_.frameSize()
        or recordQueue_.samples() < recordQueue_.frameSize()) {
        JAMI_DBG("getRecorded underflow %d / %d, %d / %d",
                 playbackQueue_.samples(),
                 playbackQueue_.frameSize(),
                 recordQueue_.samples(),
                 recordQueue_.frameSize());
        return {};
    }
    if (recordQueue_.samples() > 2 * recordQueue_.frameSize() && playbackQueue_.samples() == 0) {
        JAMI_DBG("getRecorded PLAYBACK underflow");
        return recordQueue_.dequeue();
    }
    while (playbackQueue_.samples() > 10 * playbackQueue_.frameSize()) {
        JAMI_DBG("getRecorded record underflow");
        playbackQueue_.dequeue();
    }
    while (recordQueue_.samples() > 4 * recordQueue_.frameSize()) {
        JAMI_DBG("getRecorded playback underflow");
        recordQueue_.dequeue();
    }
    auto playback = playbackQueue_.dequeue();
    auto record = recordQueue_.dequeue();
    if (playback and record) {
        auto frameSize = record->getFrameSize();
        auto format = record->getFormat();
        auto ret = std::make_shared<AudioFrame>(format, frameSize);
        // process

        // record
        pimpl_->apm->set_stream_delay_ms(0);
        pimpl_->apm->echo_cancellation()->set_stream_drift_samples(0);

        webrtc::StreamConfig sc(format.sample_rate, format.nb_channels);

        /* using fBuffer = std::vector<std::vector<float>>;
         fBuffer fRecord(format.nb_channels, std::vector<float>(frameSize));
         AudioBuffer recBuf((const AudioSample*) record->pointer()->data[0], frameSize, format);
         for (auto c = 0; c < format.nb_channels; ++c) {
             recBuf.channelToFloat(fRecord[c].data(), 0);
         }*/

        using fChannelBuffer = std::vector<float*>;

        fChannelBuffer fRecBuf(format.nb_channels);
        AudioBuffer recBuf((const AudioSample*) record->pointer()->data[0], frameSize, format);
        for (auto c = 0; c < fRecBuf.size(); ++c)
            recBuf.channelToFloat(fRecBuf[c], c);

        fChannelBuffer fPlayBuf(format.nb_channels);
        AudioBuffer playBuf((const AudioSample*) playback->pointer()->data[0], frameSize, format);
        for (auto c = 0; c < fPlayBuf.size(); ++c)
            recBuf.channelToFloat(fPlayBuf[c], c);

        if (pimpl_->apm->ProcessStream(fRecBuf.data(), sc, sc, fPlayBuf.data())
            != webrtc::AudioProcessing::kNoError)
            JAMI_ERR("ProcessStream failed");

        // play
        if (pimpl_->apm->ProcessReverseStream(fPlayBuf.data(), sc, sc, fPlayBuf.data())
            != webrtc::AudioProcessing::kNoError)
            JAMI_ERR("ProcessReverseStream failed");

        // pimpl_->state.get(),
        //                                (const int16_t*) record->pointer()->data[0],
        //                                (const int16_t*) playback->pointer()->data[0],
        //                                (int16_t*) ret->pointer()->data[0]);
        return ret;
    }
    return {};
}

void
WebRTCEchoCanceller::done()
{}

} // namespace jami
