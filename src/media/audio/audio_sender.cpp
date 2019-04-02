/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#include "audio_input.h"
#include "audio_sender.h"
#include "client/videomanager.h"
#include "libav_deps.h"
#include "logger.h"
#include "media_encoder.h"
#include "media_io_handle.h"
#include "media_stream.h"
#include "resampler.h"
#include "smartools.h"

#include <memory>

namespace jami {

AudioSender::AudioSender(const std::string& id,
                         const std::string& dest,
                         const MediaDescription& args,
                         SocketPair& socketPair,
                         const uint16_t seqVal,
                         bool muteState,
                         const uint16_t mtu) :
    id_(id),
    dest_(dest),
    args_(args),
    seqVal_(seqVal),
    muteState_(muteState),
    mtu_(mtu)
{
    setup(socketPair);
}

AudioSender::~AudioSender()
{
    audioInput_->detach(this);
    audioInput_.reset();
    audioEncoder_.reset();
    muxContext_.reset();
    micData_.clear();
    resampledData_.clear();
}

bool
AudioSender::setup(SocketPair& socketPair)
{
    audioEncoder_.reset(new MediaEncoder);
    muxContext_.reset(socketPair.createIOContext(mtu_));

    try {
        /* Encoder setup */
        JAMI_DBG("audioEncoder_->openOutput %s", dest_.c_str());
        audioEncoder_->openOutput(dest_, "rtp");
        audioEncoder_->setOptions(args_);
        auto codec = std::static_pointer_cast<AccountAudioCodecInfo>(args_.codec);
        auto ms = MediaStream("audio sender", codec->audioformat);
        audioEncoder_->setOptions(ms);
        audioEncoder_->addStream(args_.codec->systemCodecInfo);
        audioEncoder_->setInitSeqVal(seqVal_);
        audioEncoder_->setIOContext(muxContext_->getContext());
    } catch (const MediaEncoderException &e) {
        JAMI_ERR("%s", e.what());
        return false;
    }

    Smartools::getInstance().setLocalAudioCodec(audioEncoder_->getEncoderName());

#ifdef DEBUG_SDP
    audioEncoder_->print_sdp();
#endif

    // NOTE do after encoder is ready to encode
    auto codec = std::static_pointer_cast<AccountAudioCodecInfo>(args_.codec);
    audioInput_ = jami::getAudioInput(id_);
    audioInput_->setFormat(codec->audioformat);
    audioInput_->attach(this);

    return true;
}

void
AudioSender::update(Observable<std::shared_ptr<jami::MediaFrame>>* /*obs*/, const std::shared_ptr<jami::MediaFrame>& framePtr)
{
    auto frame = framePtr->pointer();
    auto ms = MediaStream("a:local", frame->format, rational<int>(1, frame->sample_rate),
                          frame->sample_rate, frame->channels, frame->nb_samples);
    frame->pts = sent_samples;
    ms.firstTimestamp = frame->pts;
    sent_samples += frame->nb_samples;

    if (audioEncoder_->encodeAudio(*std::static_pointer_cast<AudioFrame>(framePtr)) < 0)
        JAMI_ERR("encoding failed");
}

void
AudioSender::setMuted(bool isMuted)
{
    muteState_ = isMuted;
    audioInput_->setMuted(isMuted);
}

uint16_t
AudioSender::getLastSeqValue()
{
    return audioEncoder_->getLastSeqValue();
}

} // namespace jami
