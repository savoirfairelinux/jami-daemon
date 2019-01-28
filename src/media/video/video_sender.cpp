/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
#include "sip/sipcall.h"
#ifdef RING_ACCEL
#include "accel.h"
#endif

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
    keyFrameFreq_ = dev.framerate.numerator() * KEY_FRAME_PERIOD;
    videoEncoder_->openOutput(dest, "rtp");
    videoEncoder_->setDeviceOptions(dev);
    videoEncoder_->setOptions(args);
    videoEncoder_->addStream(args.codec->systemCodecInfo);
    videoEncoder_->setInitSeqVal(seqVal);
    videoEncoder_->setIOContext(muxContext_->getContext());
    videoEncoder_->startIO();

    videoEncoder_->print_sdp();

    // Send local video codec in SmartInfo
    Smartools::getInstance().setLocalVideoCodec(videoEncoder_->getEncoderName());

    // Send the resolution in smartInfo
    Smartools::getInstance().setResolution("local", dev.width, dev.height);
}

VideoSender::~VideoSender()
{
    videoEncoder_->flush();
}


void
VideoSender::encodeAndSendVideo(VideoFrame& input_frame)
{
    if (auto packet = input_frame.packet()) {
#if __ANDROID__
        if (forceKeyFrame_) {
            emitSignal<DRing::VideoSignal::RequestKeyFrame>();
            forceKeyFrame_ = 0;
        }
#endif
        videoEncoder_->send(*packet);
    } else {
        bool is_keyframe = forceKeyFrame_ > 0
            or (keyFrameFreq_ > 0 and (frameNumber_ % keyFrameFreq_) == 0);

        if (is_keyframe)
            --forceKeyFrame_;

        if (frameNumber_%300 == 0) {
            auto call = std::static_pointer_cast<SIPCall>(Manager::instance().getCurrentCall());
            if (call) {
                std::srand(time(NULL));
                call->setVideoOrientation(int(std::rand()%4)*90);
            }
        }

#ifdef RING_ACCEL
        auto framePtr = transferToMainMemory(input_frame, AV_PIX_FMT_NV12);
        auto& swFrame = *framePtr;
#else
        auto& swFrame = input_frame;
#endif
        if (videoEncoder_->encode(swFrame, is_keyframe, frameNumber_++) < 0)
            RING_ERR("encoding failed");
    }
}

void
VideoSender::update(Observable<std::shared_ptr<MediaFrame>>* /*obs*/,
                    const std::shared_ptr<MediaFrame>& frame_p)
{
    encodeAndSendVideo(*std::static_pointer_cast<VideoFrame>(frame_p));
}

void
VideoSender::forceKeyFrame()
{
    RING_DBG("Key frame requested");
    ++forceKeyFrame_;
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
