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
    , lastRTCPCheck_(std::chrono::system_clock::now())
    , timeRTCPChecking_(2)
    , timeRecheckConnection_(30)
{
    videoEncoder_->setDeviceOptions(dev);
    videoEncoder_->openOutput(dest.c_str(), args);
    videoEncoder_->setInitSeqVal(seqVal);
    videoEncoder_->setIOContext(muxContext_);
    videoEncoder_->startIO();

    videoEncoder_->print_sdp(sdp_);

    /*auto call = ring::Manager::instance().getCurrentCall();
    auto acc = ring::Manager::instance().getAccount(call->getAccountId());
    auto codecVideo =
        std::static_pointer_cast<ring::AccountVideoCodecInfo>(acc->getRunningAccountCodecInfo(MEDIA_VIDEO));
    auto map =  codecVideo->getCodecSpecifications();
    currentBitrate_= std::stoi(map[DRing::Account::ConfProperties::CodecInfo::BITRATE]);
    */
}

VideoSender::~VideoSender()
{
    videoEncoder_->flush();

}

float VideoSender::checkPeerPacketLoss()
{
    auto rtcpInfo = socketPair_.getRtcpInfo();

    return (rtcpInfo->fraction_lost / 256.0) * 100;
}

static void changeBitrate(unsigned newBitrate)
{
    auto call = ring::Manager::instance().getCurrentCall();
    auto acc = ring::Manager::instance().getAccount(call->getAccountId());
    auto codecVideo =
        std::static_pointer_cast<ring::AccountVideoCodecInfo>(acc->getRunningAccountCodecInfo(MEDIA_VIDEO));
    auto map =  codecVideo->getCodecSpecifications();
    map[DRing::Account::ConfProperties::CodecInfo::BITRATE] = std::to_string(newBitrate);
    codecVideo->setCodecSpecifications(map);
    RING_WARN("change video encoder bitrate to %d",newBitrate);
    call->restartMediaSender();
}

void VideoSender::adaptBitrate()
{
    bool needToCheckBitrate = false;
    float lossRate = 0.0;
    auto rtcpCheckTimer = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now() - lastRTCPCheck_);
    if (not hasReachIdealBitrate_) {
        if (rtcpCheckTimer.count() >= 2)
            needToCheckBitrate = true;
    }else{
        if (rtcpCheckTimer.count() >= 30)
            needToCheckBitrate = true;
        else
            return;
    }

    if(cptBitrateChecking_ >= MAX_TRY) {
        RING_WARN("reach maximal tries");
        hasReachIdealBitrate_ = true;
        cptBitrateChecking_ = 1;
    }


    if(needToCheckBitrate) {
        cptBitrateChecking_++;
        lastRTCPCheck_ = std::chrono::system_clock::now();


        if((lossRate = checkPeerPacketLoss()) > PACKET_LOSS_THRESHOLD) {
            RING_DBG("lossRate=%f decrease bitrate", lossRate);
            if (isPreviousBitrateOk_) {
                currentBitrate_ = 2 * currentBitrate_ - MAX_BITRATE;
                hasReachIdealBitrate_ = true;
            }else{
                currentBitrate_ = (currentBitrate_ + MIN_BITRATE) / 2;
            }
            runOnMainThread(std::bind(changeBitrate, currentBitrate_));
        } else {
            RING_DBG("lossRate=%f increase bitrate", lossRate);
            isPreviousBitrateOk_ = true;
            currentBitrate_ = (currentBitrate_ + MAX_BITRATE) / 2;
            if (currentBitrate_ >= MAX_BITRATE)
                currentBitrate_ = MAX_BITRATE;
            runOnMainThread(std::bind(changeBitrate, currentBitrate_));
        }
    }
}

void VideoSender::encodeAndSendVideo(VideoFrame& input_frame)
{
    bool is_keyframe = forceKeyFrame_ > 0;

    if (is_keyframe)
        --forceKeyFrame_;

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
