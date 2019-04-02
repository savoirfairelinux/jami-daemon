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
#include "media_device.h"
#include "smartools.h"
#include "sip/sipcall.h"
#ifdef RING_ACCEL
#include "accel.h"
#endif

#include <map>
#include <unistd.h>
extern "C" {
#include <libavutil/display.h>
}

namespace jami { namespace video {

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
    auto opts = MediaStream("video sender", AV_PIX_FMT_YUV420P, 1 / (rational<int>)dev.framerate, dev.width, dev.height, 1, (rational<int>)dev.framerate);
    videoEncoder_->setOptions(opts);
    videoEncoder_->setOptions(args);
    videoEncoder_->addStream(args.codec->systemCodecInfo);
    videoEncoder_->setInitSeqVal(seqVal);
    videoEncoder_->setIOContext(muxContext_->getContext());

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
        int size {0};
        uint8_t* side_data = av_packet_get_side_data(packet, AV_PKT_DATA_DISPLAYMATRIX, &size);
        auto angle = (side_data == nullptr || size == 0) ? 0 : av_display_rotation_get(reinterpret_cast<int32_t*>(side_data));
        if (rotation_ != angle) {
            rotation_ = angle;
            if (changeOrientationCallback_)
                changeOrientationCallback_(rotation_);
        }

        videoEncoder_->send(*packet);
    } else {
        bool is_keyframe = forceKeyFrame_ > 0
            or (keyFrameFreq_ > 0 and (frameNumber_ % keyFrameFreq_) == 0);

        if (is_keyframe)
            --forceKeyFrame_;

        AVFrameSideData* side_data = av_frame_get_side_data(input_frame.pointer(), AV_FRAME_DATA_DISPLAYMATRIX);
        auto angle = side_data == nullptr ? 0 : av_display_rotation_get(reinterpret_cast<int32_t*>(side_data->data));
        if (rotation_ != angle) {
            rotation_ = angle;
            if (changeOrientationCallback_)
                changeOrientationCallback_(rotation_);
        }

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
    encodeAndSendVideo(*std::static_pointer_cast<VideoFrame>(frame_p));
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

bool
VideoSender::useCodec(const jami::AccountVideoCodecInfo* codec) const
{
    return videoEncoder_->useCodec(codec);
}

void
VideoSender::setChangeOrientationCallback(std::function<void(int)> cb)
{
    changeOrientationCallback_ = cb;
}

}} // namespace jami::video
