/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Eloi Bail <eloi.bail@savoirfairelinux.com>
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

#include "video_sender.h"
#include "video_mixer.h"
#include "socket_pair.h"
#include "client/videomanager.h"
#include "logger.h"
#include "manager.h"
#include "smartools.h"

#include <map>
#include <unistd.h>

namespace ring { namespace video {

using std::string;

VideoSender::VideoSender(const std::string& dest, const DeviceParams& dev,
                         const MediaDescription& args, SocketPair& socketPair,
                         const uint16_t seqVal,
                         uint16_t mtu)
    : muxContext_(socketPair.createIOContext(mtu))
    , videoEncoder_(new MediaEncoder)
{
    videoEncoder_->setDeviceOptions(dev);
    keyFrameFreq_ = dev.framerate.numerator() * KEY_FRAME_PERIOD;
    videoEncoder_->openOutput(dest.c_str(), args);
    videoEncoder_->setInitSeqVal(seqVal);
    videoEncoder_->setIOContext(muxContext_);
    videoEncoder_->startIO();

    videoEncoder_->print_sdp();
}

VideoSender::~VideoSender()
{
    videoEncoder_->flush();
}


void
VideoSender::encodeAndSendVideo(VideoFrame& input_frame)
{
    bool is_keyframe = forceKeyFrame_ > 0
        or (keyFrameFreq_ > 0 and (frameNumber_ % keyFrameFreq_) == 0);

    if (is_keyframe)
        --forceKeyFrame_;

    if (videoEncoder_->encode(input_frame, is_keyframe, frameNumber_++) < 0)
        RING_ERR("encoding failed");

    // Send local video codec in SmartInfo
    Smartools::getInstance().setLocalVideoCodec(videoEncoder_->getEncoderName());
}

void
VideoSender::update(Observable<std::shared_ptr<VideoFrame>>* /*obs*/,
                    const std::shared_ptr<VideoFrame>& frame_p)
{
    encodeAndSendVideo(*frame_p);
}

void
VideoSender::forceKeyFrame()
{
    RING_DBG("Key frame requested");
    ++forceKeyFrame_;
}

void
VideoSender::setMuted(bool isMuted)
{
    videoEncoder_->setMuted(isMuted);
}

uint16_t
VideoSender::getLastSeqValue()
{
    return videoEncoder_->getLastSeqValue();
}

bool
VideoSender::useCodec(const ring::AccountVideoCodecInfo* codec) const
{
    return videoEncoder_->useCodec(codec);
}

}} // namespace ring::video
