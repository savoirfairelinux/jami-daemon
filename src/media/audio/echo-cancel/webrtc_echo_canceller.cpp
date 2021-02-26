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
    EchoCanceller::putRecorded(std::move(buf));
}

void
WebRTCEchoCanceller::putPlayback(const std::shared_ptr<AudioFrame>& buf)
{
    EchoCanceller::putPlayback(buf);
}

std::shared_ptr<AudioFrame>
WebRTCEchoCanceller::getProcessed()
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
        JAMI_DBG("underflow rec: %d, play: %d fs: %d",
                 recordQueue_.samples(),
                 playbackQueue_.samples(),
                 frameSize_);
        return {};
    }

    JAMI_WARN("Processing %d samples, rec: %d, play: %d ",
              frameSize_,
              recordQueue_.samples(),
              playbackQueue_.samples());

    auto playback = playbackQueue_.dequeue();
    auto record = recordQueue_.dequeue();
    if (playback and record) {
        ////////////////////////
        // process using deinterleaved float
        ///////////////////////
        
        auto format = record->getFormat();
        auto ret = std::make_shared<AudioFrame>(format, frameSize_);
        pimpl_->apm->set_stream_delay_ms(0);
        pimpl_->apm->echo_cancellation()->set_stream_drift_samples(0);

        webrtc::StreamConfig sc(format.sample_rate, format.nb_channels);

        // play
        std::vector<std::vector<float>> fPlayBuf(format.nb_channels, std::vector<float>(frameSize_));
        std::vector<float*> fPlayBufPtrs(fPlayBuf.size());
        AudioBuffer playBuf((const AudioSample*) playback->pointer()->data[0], frameSize_, format);
        for (auto c = 0; c < format.nb_channels; ++c) {
            playBuf.channelToFloat(fPlayBuf[c].data(), c);
            fPlayBufPtrs[c] = &*fPlayBuf[c].begin();
        }

        if (pimpl_->apm->ProcessReverseStream(fPlayBufPtrs.data(), sc, sc, fPlayBufPtrs.data())
            != webrtc::AudioProcessing::kNoError)
            JAMI_ERR("ProcessReverseStream failed");

        // rec
        std::vector<std::vector<float>> fRecBuf(format.nb_channels, std::vector<float>(frameSize_));
        std::vector<float*> fRecBufPtrs(fRecBuf.size());
        AudioBuffer recBuf((const AudioSample*) record->pointer()->data[0], frameSize_, format);
        for (auto c = 0; c < format.nb_channels; ++c) {
            recBuf.channelToFloat(fRecBuf[c].data(), c);
            fRecBufPtrs[c] = &*fRecBuf[c].begin();
        }

        if (pimpl_->apm->ProcessStream(fRecBufPtrs.data(), sc, sc, fRecBufPtrs.data())
            != webrtc::AudioProcessing::kNoError)
            JAMI_ERR("ProcessStream failed");

        recBuf.convertFloatPlanarToSigned16((uint8_t**) fRecBufPtrs.data(),
                                            frameSize_,
                                            format.nb_channels);
        recBuf.interleave((AudioSample*) ret->pointer()->extended_data[0]);       

        return ret;
    }
    return {};
}

void
WebRTCEchoCanceller::done()
{}

} // namespace jami
