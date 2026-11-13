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

#include "video_sender.h"
#include "socket_pair.h"
#include "logger.h"
#include "manager.h"

#ifdef ENABLE_HWACCEL
#include "accel.h"
#endif

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
    videoEncoder_->openOutput(dest, "rtp");
    videoEncoder_->setOptions(opts);
    videoEncoder_->setOptions(args);
#ifdef ENABLE_HWACCEL
    videoEncoder_->enableAccel(enableHwAccel and Manager::instance().videoPreferences.getEncodingAccelerated());
#endif
    videoEncoder_->addStream(*args.codec);
    videoEncoder_->setInitSeqVal(seqVal);
    if (const auto localSsrc = socketPair.getLocalSsrc())
        videoEncoder_->setInitSsrc(*localSsrc);
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

    if (auto* packet = input_frame->packet()) {
        // Pre-encoded packets (hardware encoders on mobile) are stamped in
        // microseconds.
        videoEncoder_->setPassthrough(true);
        videoEncoder_->send(*packet, -1, AVRational {1, 1'000'000});
    } else {
        videoEncoder_->setPassthrough(false);
        int pendingKeyFrames = forceKeyFrame_.load(std::memory_order_relaxed);
        while (pendingKeyFrames > 0
               && !forceKeyFrame_.compare_exchange_weak(pendingKeyFrames,
                                                        pendingKeyFrames - 1,
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_relaxed)) {
        }
        const bool is_keyframe = pendingKeyFrames > 0;

        if (videoEncoder_->encode(input_frame, is_keyframe, frameNumber_++) < 0)
            JAMI_ERROR("encoding failed");
    }
#ifdef DEBUG_SDP
    if (frameNumber_ == 1) // video stream is lazy initialized, wait for first frame
        videoEncoder_->print_sdp();
#endif
}

void
VideoSender::update(Observable<std::shared_ptr<MediaFrame>>* /*obs*/, const std::shared_ptr<MediaFrame>& frame_p)
{
    encodeAndSendVideo(std::dynamic_pointer_cast<VideoFrame>(frame_p));
}

void
VideoSender::forceKeyFrame()
{
    JAMI_LOG("Key frame requested");
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

int
VideoSender::getWidth() const
{
    return videoEncoder_ ? videoEncoder_->getWidth() : 0;
}

int
VideoSender::getHeight() const
{
    return videoEncoder_ ? videoEncoder_->getHeight() : 0;
}

MediaStream
VideoSender::getStream(const std::string& name) const
{
    return videoEncoder_ ? videoEncoder_->getStream(name) : MediaStream {};
}

} // namespace video
} // namespace jami
