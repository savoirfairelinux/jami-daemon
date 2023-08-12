/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include "audio_sender.h"
#include "client/videomanager.h"
#include "libav_deps.h"
#include "logger.h"
#include "media_encoder.h"
#include "media_io_handle.h"
#include "media_stream.h"
#include "resampler.h"

#include <memory>

namespace jami {

AudioSender::AudioSender(const std::string& dest,
                         const MediaDescription& args,
                         SocketPair& socketPair,
                         const uint16_t seqVal,
                         const uint16_t mtu)
    : dest_(dest)
    , args_(args)
    , seqVal_(seqVal)
    , mtu_(mtu)
{
    setup(socketPair);
}

AudioSender::~AudioSender()
{
    audioEncoder_.reset();
    muxContext_.reset();
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
        auto codec = std::static_pointer_cast<SystemAudioCodecInfo>(args_.codec);
        auto ms = MediaStream("audio sender", codec->audioformat);
        audioEncoder_->setOptions(ms);
        audioEncoder_->addStream(*args_.codec);
        audioEncoder_->setInitSeqVal(seqVal_);
        audioEncoder_->setIOContext(muxContext_->getContext());
    } catch (const MediaEncoderException& e) {
        JAMI_ERR("%s", e.what());
        return false;
    }
#ifdef DEBUG_SDP
    audioEncoder_->print_sdp();
#endif

    return true;
}

void
AudioSender::update(Observable<std::shared_ptr<jami::MediaFrame>>* /*obs*/,
                    const std::shared_ptr<jami::MediaFrame>& framePtr)
{
    auto frame = framePtr->pointer();
    frame->pts = sent_samples;
    sent_samples += frame->nb_samples;

    // check for change in voice activity, if so, call callback
    // downcast MediaFrame to AudioFrame
    bool hasVoice = std::dynamic_pointer_cast<AudioFrame>(framePtr)->has_voice;
    if (hasVoice != voice_) {
        voice_ = hasVoice;
        if (voiceCallback_) {
            voiceCallback_(voice_);
        } else {
            JAMI_ERR("AudioSender no voice callback!");
        }
    }

    if (audioEncoder_->encodeAudio(*std::static_pointer_cast<AudioFrame>(framePtr)) < 0)
        JAMI_ERR("encoding failed");
}

void
AudioSender::setVoiceCallback(std::function<void(bool)> cb)
{
    if (cb) {
        voiceCallback_ = std::move(cb);
    } else {
        JAMI_ERR("AudioSender trying to set invalid voice callback");
    }
}

uint16_t
AudioSender::getLastSeqValue()
{
    return audioEncoder_->getLastSeqValue();
}

int
AudioSender::setPacketLoss(uint64_t pl)
{
    // The encoder may be destroy during a bitrate change
    // when a codec parameter like auto quality change
    if (!audioEncoder_)
        return -1; // NOK

    return audioEncoder_->setPacketLoss(pl);
}

} // namespace jami
