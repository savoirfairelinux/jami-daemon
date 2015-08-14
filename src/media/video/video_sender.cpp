/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "video_sender.h"
#include "video_mixer.h"
#include "socket_pair.h"
#include "client/videomanager.h"
#include "logger.h"
#include "manager.h"
#include "account_const.h"

#include <map>
#include <unistd.h>

namespace ring { namespace video {

using std::string;

VideoSender::VideoSender(const std::string& dest, const DeviceParams& dev,
                         const MediaDescription& args, SocketPair& socketPair,
                         const uint16_t seqVal)
    : muxContext_(socketPair.createIOContext())
    , videoEncoder_(new MediaEncoder)
    , socketPair_(socketPair)
    , call_(ring::Manager::instance().getCurrentCall())
    , lastRTCPCheck_(std::chrono::system_clock::now())
    , lastLongRTCPCheck_(std::chrono::system_clock::now())
{
    videoEncoder_->setDeviceOptions(dev);
    keyFrameFreq_ = dev.framerate.numerator() * KEY_FRAME_PERIOD;
    videoEncoder_->openOutput(dest.c_str(), args);
    videoEncoder_->setInitSeqVal(seqVal);
    videoEncoder_->setIOContext(muxContext_);
    videoEncoder_->startIO();

    videoEncoder_->print_sdp(sdp_);
}

VideoSender::~VideoSender()
{
    videoEncoder_->flush();

}

float VideoSender::checkPeerPacketLoss()
{
    auto rtcpInfo = socketPair_.getRtcpInfo();

    return (rtcpInfo.fraction_lost / 256.0) * 100;
}

static void restartMediaEncoder(std::shared_ptr<Call> call)
{
    call->restartMediaSender();
}

void VideoSender::adaptBitrate()
{
    bool needToCheckBitrate = false;
    float packetLostRate = 0.0;

    VideoBitrateInfo videoBitrateInfo;

    auto rtcpCheckTimer = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now() - lastRTCPCheck_);
    auto rtcpLongCheckTimer = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now() - lastLongRTCPCheck_);

    if (rtcpCheckTimer.count() >= RTCP_CHECKING_INTERVAL) {
        needToCheckBitrate = true;
        lastRTCPCheck_ = std::chrono::system_clock::now();
        videoBitrateInfo = call_->getVideoBitrateInfo();
    }

    if (rtcpLongCheckTimer.count() >= RTCP_LONG_CHECKING_INTERVAL) {
        needToCheckBitrate = true;
        lastLongRTCPCheck_ = std::chrono::system_clock::now();
        videoBitrateInfo = call_->getVideoBitrateInfo();
        // we force iterative bitrate adaptation
        videoBitrateInfo.cptBitrateChecking = 0;
    }


    if (needToCheckBitrate) {
        videoBitrateInfo.cptBitrateChecking++;
        auto oldBitrate = videoBitrateInfo.videoBitrateCurrent;

        // too much packet lost : decrease bitrate
        if((packetLostRate = checkPeerPacketLoss()) >= videoBitrateInfo.packetLostThreshold) {

            // calculate new bitrate by dichotomie
            videoBitrateInfo.videoBitrateCurrent =
                ( videoBitrateInfo.videoBitrateCurrent + videoBitrateInfo.videoBitrateMin) / 2;

            // boundaries low
            if ( videoBitrateInfo.videoBitrateCurrent < videoBitrateInfo.videoBitrateMin)
                videoBitrateInfo.videoBitrateCurrent = videoBitrateInfo.videoBitrateMin;

            RING_WARN("packetLostRate=%f >= %f -> decrease bitrate",
                    packetLostRate, videoBitrateInfo.packetLostThreshold);

            // we force iterative bitrate adaptation
            videoBitrateInfo.cptBitrateChecking = 0;
            call_->setVideoBitrateInfo(videoBitrateInfo);

            // asynchronous A/V media restart
            if (videoBitrateInfo.videoBitrateCurrent != oldBitrate)
                runOnMainThread(std::bind(restartMediaEncoder, call_));

        // no packet lost: increase bitrate
        } else if (videoBitrateInfo.cptBitrateChecking <= videoBitrateInfo.maxBitrateChecking) {

            // calculate new bitrate by dichotomie
            videoBitrateInfo.videoBitrateCurrent =
                ( videoBitrateInfo.videoBitrateCurrent + videoBitrateInfo.videoBitrateMax) / 2;

            // boundaries high
            if ( videoBitrateInfo.videoBitrateCurrent > videoBitrateInfo.videoBitrateMax)
                videoBitrateInfo.videoBitrateCurrent = videoBitrateInfo.videoBitrateMax;

            RING_WARN("[%u/%u] packetLostRate=%f < %f -> try to increase bitrate",
                    videoBitrateInfo.cptBitrateChecking,
                    videoBitrateInfo.maxBitrateChecking,
                    packetLostRate,
                    videoBitrateInfo.packetLostThreshold);

            call_->setVideoBitrateInfo(videoBitrateInfo);

            // asynchronous A/V media restart
            if (videoBitrateInfo.videoBitrateCurrent != oldBitrate)
                runOnMainThread(std::bind(restartMediaEncoder, call_));

        }else{
            //nothing we reach maximal tries
        }
    }
}

void VideoSender::encodeAndSendVideo(VideoFrame& input_frame)
{
    bool is_keyframe = forceKeyFrame_ > 0;

    if (is_keyframe)
        --forceKeyFrame_;

    if (frameNumber_ < 3)
        is_keyframe = true;

    if((frameNumber_ % keyFrameFreq_) == 0) {
        RING_WARN("force key frame");
        is_keyframe = true;
    }

    if (videoEncoder_->encode(input_frame, is_keyframe, frameNumber_++) < 0)
        RING_ERR("encoding failed");
}

void VideoSender::update(Observable<std::shared_ptr<VideoFrame> >* /*obs*/,
                         std::shared_ptr<VideoFrame> & frame_p)
{
    adaptBitrate();
    encodeAndSendVideo(*frame_p);
}

void VideoSender::forceKeyFrame()
{
    ++forceKeyFrame_;
}

void VideoSender::setMuted(bool isMuted)
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
