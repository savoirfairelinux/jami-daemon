/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Eloi Bail <eloi.bail@savoirfairelinux.com>
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

#include "video_sender.h"
#include "video_mixer.h"
#include "socket_pair.h"
#include "client/videomanager.h"
#include "logger.h"
#include "manager.h"
#include "media_device.h"
#include "sip/sipcall.h"
#ifdef RING_ACCEL
#include "accel.h"
#endif

#include <map>
#include <unistd.h>

namespace jami {
namespace video {

using std::string;

VideoSender::VideoSender(const std::string& dest,
                         const MediaStream& opts,
                         const MediaDescription& args,
                         SocketPair& socketPair,
                         const uint16_t seqVal,
                         uint16_t mtu,
                         bool enableHwAccel)
    : muxContext_(socketPair.createIOContext(mtu))
    , videoEncoder_(new MediaEncoder)
{
    keyFrameFreq_ = opts.frameRate.numerator() * KEY_FRAME_PERIOD;
    videoEncoder_->openOutput(dest, "rtp");
    videoEncoder_->setOptions(opts);
    videoEncoder_->setOptions(args);
#ifdef RING_ACCEL
    videoEncoder_->enableAccel(enableHwAccel
                               and Manager::instance().videoPreferences.getEncodingAccelerated());
#endif
    videoEncoder_->addStream(args.codec->systemCodecInfo);
    videoEncoder_->setInitSeqVal(seqVal);
    videoEncoder_->setIOContext(muxContext_->getContext());
}

void
VideoSender::encodeAndSendVideo(const std::shared_ptr<VideoFrame>& input_frame)
{
    int angle = input_frame->getOrientation();
    if (rotation_ != angle) {
        rotation_ = angle;
        if (changeOrientationCallback_)
            changeOrientationCallback_(rotation_);
    }

    if (auto packet = input_frame->packet()) {
        videoEncoder_->send(*packet);
    } else {
        bool is_keyframe = forceKeyFrame_ > 0
                           or (keyFrameFreq_ > 0 and (frameNumber_ % keyFrameFreq_) == 0);

        if (is_keyframe)
            --forceKeyFrame_;

        if (videoEncoder_->encode(input_frame, is_keyframe, frameNumber_++) < 0)
            JAMI_ERR("encoding failed");
    }
#ifdef DEBUG_SDP
    if (frameNumber_ == 1) // video stream is lazy initialized, wait for first frame
        videoEncoder_->print_sdp();
#endif
}

void
VideoSender::update(Observable<std::shared_ptr<MediaFrame>>* /*obs*/,
                    const std::shared_ptr<MediaFrame>& frame_p)
{
    encodeAndSendVideo(std::dynamic_pointer_cast<VideoFrame>(frame_p));
}

void
VideoSender::forceKeyFrame()
{
    JAMI_DBG("Key frame requested");
    ++forceKeyFrame_;
}

uint16_t
VideoSender::getLastSeqValue()
{
    return videoEncoder_->getLastSeqValue();
}

void
VideoSender::setChangeOrientationCallback(std::function<void(int)> cb)
{
    changeOrientationCallback_ = std::move(cb);
}

int
VideoSender::setBitrate(uint64_t br)
{
    // The encoder may be destroy during a bitrate change
    // when a codec parameter like auto quality change
    if (!videoEncoder_)
        return -1; // NOK

    return videoEncoder_->setBitrate(br);
}

} // namespace video
} // namespace jami
