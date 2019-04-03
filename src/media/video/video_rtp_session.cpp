/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
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
 */

#include "client/videomanager.h"
#include "video_rtp_session.h"
#include "video_sender.h"
#include "video_receive_thread.h"
#include "video_mixer.h"
#include "ice_socket.h"
#include "socket_pair.h"
#include "sip/sipvoiplink.h" // for enqueueKeyframeRequest
#include "manager.h"
#include "logger.h"
#include "string_utils.h"
#include "call.h"
#include "conference.h"

#include "account_const.h"

#include <sstream>
#include <map>
#include <string>
#include <thread>
#include <chrono>

namespace jami { namespace video {

using std::map;
using std::string;

// how long (in seconds) to wait before rechecking for packet loss
static constexpr auto RTCP_PACKET_LOSS_INTERVAL = std::chrono::milliseconds(1000);

VideoRtpSession::VideoRtpSession(const string &callID,
                                 const DeviceParams& localVideoParams) :
    RtpSession(callID), localVideoParams_(localVideoParams)
    , lastRTCPCheck_(std::chrono::system_clock::now())
    , lastLongRTCPCheck_(std::chrono::system_clock::now())
    , videoBitrateInfo_ {}
    , rtcpCheckerThread_([] { return true; },
            [this]{ processRtcpChecker(); },
            []{})
    , packetLossThread_([] { return true; },
            [this]{ processPacketLoss(); },
            []{})
{
    setupVideoBitrateInfo(); // reset bitrate
}

VideoRtpSession::~VideoRtpSession()
{ stop(); }

/// Setup internal VideoBitrateInfo structure from media descriptors.
///
void
VideoRtpSession::updateMedia(const MediaDescription& send, const MediaDescription& receive)
{
    BaseType::updateMedia(send, receive);
    setupVideoBitrateInfo();
}

void
VideoRtpSession::setRequestKeyFrameCallback(std::function<void(void)> cb)
{
    requestKeyFrameCallback_ = std::move(cb);
}

void VideoRtpSession::startSender()
{
    if (send_.enabled and not send_.holding) {
        if (sender_) {
            if (videoLocal_)
                videoLocal_->detach(sender_.get());
            if (videoMixer_)
                videoMixer_->detach(sender_.get());
            JAMI_WARN("Restarting video sender");
        }

        if (not conference_) {
            videoLocal_ = getVideoCamera();
            if (auto input = Manager::instance().getVideoManager().videoInput.lock()) {
                auto newParams = input->switchInput(input_);
                try {
                    if (newParams.valid() &&
                        newParams.wait_for(NEWPARAMS_TIMEOUT) == std::future_status::ready) {
                        localVideoParams_ = newParams.get();
                    } else {
                        JAMI_ERR("No valid new video parameters.");
                        return;
                    }
                } catch (const std::exception& e) {
                    JAMI_ERR("Exception during retrieving video parameters: %s",
                             e.what());
                    return;
                }
            } else {
                JAMI_WARN("Can't lock video input");
                return;
            }
        }


        // be sure to not send any packets before saving last RTP seq value
        socketPair_->stopSendOp();
        if (sender_)
            initSeqVal_ = sender_->getLastSeqValue() + 1;
        try {
            sender_.reset();
            socketPair_->stopSendOp(false);
            sender_.reset(new VideoSender(getRemoteRtpUri(), localVideoParams_,
                                          send_, *socketPair_, initSeqVal_, mtu_));
            if (changeOrientationCallback_)
                sender_->setChangeOrientationCallback(changeOrientationCallback_);

        } catch (const MediaEncoderException &e) {
            JAMI_ERR("%s", e.what());
            send_.enabled = false;
        }
        auto codecVideo = std::static_pointer_cast<jami::AccountVideoCodecInfo>(send_.codec);
        auto autoQuality = codecVideo->isAutoQualityEnabled;
        if (autoQuality and not rtcpCheckerThread_.isRunning())
            rtcpCheckerThread_.start();
        else if (not autoQuality and rtcpCheckerThread_.isRunning())
            rtcpCheckerThread_.join();
    }
}

void
VideoRtpSession::restartSender()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // ensure that start has been called before restart
    if (not socketPair_)
        return;

    startSender();
    setupVideoPipeline();
}

void VideoRtpSession::startReceiver()
{
    if (receive_.enabled and not receive_.holding) {
        if (receiveThread_)
            JAMI_WARN("Restarting video receiver");
        receiveThread_.reset(
            new VideoReceiveThread(callID_, receive_.receiving_sdp, mtu_)
        );

        // XXX keyframe requests can timeout if unanswered
        receiveThread_->setRequestKeyFrameCallback(requestKeyFrameCallback_);
        receiveThread_->addIOContext(*socketPair_);
        receiveThread_->startLoop();
        packetLossThread_.start();
    } else {
        JAMI_DBG("Video receiving disabled");
        if (receiveThread_)
            receiveThread_->detach(videoMixer_.get());
        receiveThread_.reset();
        packetLossThread_.join();
    }
}

void VideoRtpSession::start(std::unique_ptr<IceSocket> rtp_sock,
                            std::unique_ptr<IceSocket> rtcp_sock)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (not send_.enabled and not receive_.enabled) {
        stop();
        return;
    }

    try {
        if (rtp_sock and rtcp_sock)
            socketPair_.reset(new SocketPair(std::move(rtp_sock), std::move(rtcp_sock)));
        else
            socketPair_.reset(new SocketPair(getRemoteRtpUri().c_str(), receive_.addr.getPort()));

        if (send_.crypto and receive_.crypto) {
            socketPair_->createSRTP(receive_.crypto.getCryptoSuite().c_str(),
                                    receive_.crypto.getSrtpKeyInfo().c_str(),
                                    send_.crypto.getCryptoSuite().c_str(),
                                    send_.crypto.getSrtpKeyInfo().c_str());
        }
    } catch (const std::runtime_error& e) {
        JAMI_ERR("Socket creation failed: %s", e.what());
        return;
    }

    startSender();
    startReceiver();

    setupVideoPipeline();
}

void VideoRtpSession::stop()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    rtcpCheckerThread_.join();
    packetLossThread_.join();
    if (videoLocal_)
        videoLocal_->detach(sender_.get());

    if (videoMixer_) {
        videoMixer_->detach(sender_.get());
        if (receiveThread_)
            receiveThread_->detach(videoMixer_.get());
    }

    if (socketPair_)
        socketPair_->interrupt();

    // reset default video quality if exist
    if (videoBitrateInfo_.videoQualityCurrent != SystemCodecInfo::DEFAULT_NO_QUALITY)
        videoBitrateInfo_.videoQualityCurrent = SystemCodecInfo::DEFAULT_CODEC_QUALITY;

    videoBitrateInfo_.videoBitrateCurrent = SystemCodecInfo::DEFAULT_VIDEO_BITRATE;
    storeVideoBitrateInfo();

    receiveThread_.reset();
    sender_.reset();
    socketPair_.reset();
    videoLocal_.reset();
}

void VideoRtpSession::forceKeyFrame()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (sender_)
        sender_->forceKeyFrame();
}

void
VideoRtpSession::setRotation(int rotation)
{
    receiveThread_->setRotation(rotation);
}

void
VideoRtpSession::setupVideoPipeline()
{
    if (conference_)
        setupConferenceVideoPipeline(*conference_);
    else if (sender_) {
        if (videoLocal_)
            videoLocal_->attach(sender_.get());
    } else {
        videoLocal_.reset();
    }
}

void
VideoRtpSession::setupConferenceVideoPipeline(Conference& conference)
{
    JAMI_DBG("[call:%s] Setup video pipeline on conference %s", callID_.c_str(),
             conference.getConfID().c_str());
    videoMixer_ = conference.getVideoMixer();

    if (sender_) {
        // Swap sender from local video to conference video mixer
        if (videoLocal_)
            videoLocal_->detach(sender_.get());
        videoMixer_->attach(sender_.get());
    } else
        JAMI_WARN("[call:%s] no sender", callID_.c_str());

    if (receiveThread_) {
        receiveThread_->enterConference();
        receiveThread_->attach(videoMixer_.get());
    } else
        JAMI_WARN("[call:%s] no receiver", callID_.c_str());
}

void
VideoRtpSession::enterConference(Conference* conference)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    exitConference();

    conference_ = conference;
    JAMI_DBG("[call:%s] enterConference (conf: %s)", callID_.c_str(),
             conference->getConfID().c_str());

    if (send_.enabled or receiveThread_) {
        videoMixer_ = conference->getVideoMixer();
        videoMixer_->setDimensions(localVideoParams_.width, localVideoParams_.height);
        setupConferenceVideoPipeline(*conference_);
    }
}

void VideoRtpSession::exitConference()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!conference_)
        return;

    JAMI_DBG("[call:%s] exitConference (conf: %s)", callID_.c_str(),
             conference_->getConfID().c_str());

    if (videoMixer_) {
        if (sender_)
            videoMixer_->detach(sender_.get());

        if (receiveThread_) {
            receiveThread_->detach(videoMixer_.get());
            receiveThread_->exitConference();
        }

        videoMixer_.reset();
    }

    if (videoLocal_)
        videoLocal_->attach(sender_.get());

    conference_ = nullptr;
}

bool
VideoRtpSession::useCodec(const jami::AccountVideoCodecInfo* codec) const
{
    return sender_->useCodec(codec);
}


float
VideoRtpSession::checkPeerPacketLoss()
{
    auto rtcpInfoVect = socketPair_->getRtcpInfo();
    unsigned totalLost = 0;
    unsigned fract = 0;
    auto vectSize = rtcpInfoVect.size();

    for (const auto& it : rtcpInfoVect) {
        fract = (ntohl(it.fraction_lost) & 0xff000000)  >> 24;
        totalLost += fract;
    }

    if (vectSize != 0)
        return (float)( 100 * totalLost) / (256.0 * vectSize);
    else
        return NO_PACKET_LOSS_CALCULATED;
}

unsigned
VideoRtpSession::getLowerQuality()
{
    // if lower quality was stored we return it
    unsigned quality = 0;
    while ( not histoQuality_.empty()) {
        quality = histoQuality_.back();
        histoQuality_.pop_back();
        if (quality > videoBitrateInfo_.videoQualityCurrent)
            return quality;
    }

    // if no appropriate quality found, calculate it with dichotomie
    quality = (videoBitrateInfo_.videoQualityCurrent + videoBitrateInfo_.videoQualityMin) / 2;
    return quality;
}

unsigned
VideoRtpSession::getLowerBitrate()
{
    // if a lower bitrate was stored we return it
    unsigned bitrate = 0;
    while ( not histoBitrate_.empty()) {
        bitrate = histoBitrate_.back();
        histoBitrate_.pop_back();
        if (bitrate < videoBitrateInfo_.videoBitrateCurrent)
            return bitrate;
    }

    // if no appropriate bitrate found, calculate it with dichotomie
    bitrate = (videoBitrateInfo_.videoBitrateCurrent + videoBitrateInfo_.videoBitrateMin) / 2;
    return bitrate;
}

void
VideoRtpSession::adaptQualityAndBitrate()
{
    bool needToCheckQuality = false;
    bool mediaRestartNeeded = false;
    float packetLostRate = 0.0;

    auto rtcpCheckTimer = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now() - lastRTCPCheck_);
    auto rtcpLongCheckTimer = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now() - lastLongRTCPCheck_);

    if (rtcpCheckTimer.count() >= RTCP_CHECKING_INTERVAL) {
        needToCheckQuality = true;
        lastRTCPCheck_ = std::chrono::system_clock::now();
    }

    if (rtcpLongCheckTimer.count() >= RTCP_LONG_CHECKING_INTERVAL) {
        needToCheckQuality = true;
        hasReachMaxQuality_ = false;
        lastLongRTCPCheck_ = std::chrono::system_clock::now();
        // we force iterative bitrate adaptation
        videoBitrateInfo_.cptBitrateChecking = 0;
    }


    if (needToCheckQuality) {

        videoBitrateInfo_.cptBitrateChecking++;

        // packetLostRate is not already available. Do nothing
        if ((packetLostRate = checkPeerPacketLoss()) == NO_PACKET_LOSS_CALCULATED) {
            // we force iterative bitrate adaptation
            videoBitrateInfo_.cptBitrateChecking = 0;

        // too much packet lost : decrease quality and bitrate
        } else if (packetLostRate >= videoBitrateInfo_.packetLostThreshold) {

            // calculate new quality by dichotomie
            videoBitrateInfo_.videoQualityCurrent = getLowerQuality();

            // calculate new bitrate by dichotomie
            videoBitrateInfo_.videoBitrateCurrent =  getLowerBitrate();

            // boundaries low
            if (videoBitrateInfo_.videoQualityCurrent > videoBitrateInfo_.videoQualityMin)
                videoBitrateInfo_.videoQualityCurrent = videoBitrateInfo_.videoQualityMin;

            if (videoBitrateInfo_.videoBitrateCurrent < videoBitrateInfo_.videoBitrateMin)
                videoBitrateInfo_.videoBitrateCurrent = videoBitrateInfo_.videoBitrateMin;


            // we force iterative bitrate and quality adaptation
            videoBitrateInfo_.cptBitrateChecking = 0;

            // asynchronous A/V media restart
            // we give priority to quality
            if (((videoBitrateInfo_.videoQualityCurrent != SystemCodecInfo::DEFAULT_NO_QUALITY) &&
                    (videoBitrateInfo_.videoQualityCurrent !=  (histoQuality_.empty() ? 0 : histoQuality_.back()))) ||
                ((videoBitrateInfo_.videoQualityCurrent == SystemCodecInfo::DEFAULT_NO_QUALITY) &&
                    (videoBitrateInfo_.videoBitrateCurrent !=  (histoBitrate_.empty() ? 0 : histoBitrate_.back())))) {
                mediaRestartNeeded = true;
                hasReachMaxQuality_ = true;
            }


        // no packet lost: increase quality and bitrate
        } else if ((videoBitrateInfo_.cptBitrateChecking <= videoBitrateInfo_.maxBitrateChecking)
                   and not hasReachMaxQuality_) {

            // calculate new quality by dichotomie
            videoBitrateInfo_.videoQualityCurrent =
                (videoBitrateInfo_.videoQualityCurrent + videoBitrateInfo_.videoQualityMax) / 2;

            // calculate new bitrate by dichotomie
            videoBitrateInfo_.videoBitrateCurrent =
                ( videoBitrateInfo_.videoBitrateCurrent + videoBitrateInfo_.videoBitrateMax) / 2;

            // boundaries high
            if (videoBitrateInfo_.videoQualityCurrent < videoBitrateInfo_.videoQualityMax)
                videoBitrateInfo_.videoQualityCurrent = videoBitrateInfo_.videoQualityMax;

            if (videoBitrateInfo_.videoBitrateCurrent > videoBitrateInfo_.videoBitrateMax)
                videoBitrateInfo_.videoBitrateCurrent = videoBitrateInfo_.videoBitrateMax;

            // asynchronous A/V media restart
            // we give priority to quality
            if (((videoBitrateInfo_.videoQualityCurrent != SystemCodecInfo::DEFAULT_NO_QUALITY) &&
                    (videoBitrateInfo_.videoQualityCurrent !=  (histoQuality_.empty() ? 0 : histoQuality_.back()))) ||
                ((videoBitrateInfo_.videoQualityCurrent == SystemCodecInfo::DEFAULT_NO_QUALITY) &&
                    (videoBitrateInfo_.videoBitrateCurrent !=  (histoBitrate_.empty() ? 0 : histoBitrate_.back()))))
                mediaRestartNeeded = true;

            if (videoBitrateInfo_.cptBitrateChecking == videoBitrateInfo_.maxBitrateChecking)
                lastLongRTCPCheck_ = std::chrono::system_clock::now();

        } else {
            // nothing we reach maximal tries
        }
    }

    if (mediaRestartNeeded) {
        storeVideoBitrateInfo();
        const auto& cid = callID_;

        JAMI_WARN("[%u/%u] packetLostRate=%f -> change quality to %d bitrate to %d",
                videoBitrateInfo_.cptBitrateChecking,
                videoBitrateInfo_.maxBitrateChecking,
                packetLostRate,
                videoBitrateInfo_.videoQualityCurrent,
                videoBitrateInfo_.videoBitrateCurrent);

        runOnMainThread([cid]{
            if (auto call = Manager::instance().callFactory.getCall(cid))
                call->restartMediaSender();
            });
    }
}

void
VideoRtpSession::setupVideoBitrateInfo() {
    auto codecVideo = std::static_pointer_cast<jami::AccountVideoCodecInfo>(send_.codec);
    if (codecVideo) {
        videoBitrateInfo_ = {
            (unsigned)(jami::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::BITRATE])),
            (unsigned)(jami::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE])),
            (unsigned)(jami::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE])),
            (unsigned)(jami::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::QUALITY])),
            (unsigned)(jami::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::MIN_QUALITY])),
            (unsigned)(jami::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::MAX_QUALITY])),
            videoBitrateInfo_.cptBitrateChecking,
            videoBitrateInfo_.maxBitrateChecking,
            videoBitrateInfo_.packetLostThreshold,
        };
    } else {
        videoBitrateInfo_ = {0, 0, 0, 0, 0, 0, 0,
                             MAX_ADAPTATIVE_BITRATE_ITERATION,
                             PACKET_LOSS_THRESHOLD};
    }
}

void
VideoRtpSession::storeVideoBitrateInfo() {
    auto codecVideo = std::static_pointer_cast<jami::AccountVideoCodecInfo>(send_.codec);

    if (codecVideo) {
        codecVideo->setCodecSpecifications({
            {DRing::Account::ConfProperties::CodecInfo::BITRATE, jami::to_string(videoBitrateInfo_.videoBitrateCurrent)},
            {DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE, jami::to_string(videoBitrateInfo_.videoBitrateMin)},
            {DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE, jami::to_string(videoBitrateInfo_.videoBitrateMax)},
            {DRing::Account::ConfProperties::CodecInfo::QUALITY, jami::to_string(videoBitrateInfo_.videoQualityCurrent)},
            {DRing::Account::ConfProperties::CodecInfo::MIN_QUALITY, jami::to_string(videoBitrateInfo_.videoQualityMin)},
            {DRing::Account::ConfProperties::CodecInfo::MAX_QUALITY, jami::to_string(videoBitrateInfo_.videoQualityMax)}
        });

        if (histoQuality_.size() > MAX_SIZE_HISTO_QUALITY_)
            histoQuality_.pop_front();

        if (histoBitrate_.size() > MAX_SIZE_HISTO_BITRATE_)
            histoBitrate_.pop_front();

        histoQuality_.push_back(videoBitrateInfo_.videoQualityCurrent);
        histoBitrate_.push_back(videoBitrateInfo_.videoBitrateCurrent);

    }
}

void
VideoRtpSession::processRtcpChecker()
{
    adaptQualityAndBitrate();
    rtcpCheckerThread_.wait_for(std::chrono::seconds(RTCP_CHECKING_INTERVAL));
}

void
VideoRtpSession::processPacketLoss()
{
    if (packetLossThread_.wait_for(RTCP_PACKET_LOSS_INTERVAL,
                                   [this]{return socketPair_->rtcpPacketLossDetected();})) {
        if (requestKeyFrameCallback_)
            requestKeyFrameCallback_();
    }
}

void
VideoRtpSession::initRecorder(std::shared_ptr<MediaRecorder>& rec)
{
    if (receiveThread_) {
        if (auto ob = rec->addStream(receiveThread_->getInfo())) {
            receiveThread_->attach(ob);
        }
    }
    if (auto input = std::static_pointer_cast<VideoInput>(videoLocal_)) {
        if (auto ob = rec->addStream(input->getInfo())) {
            input->attach(ob);
        }
    }
}

void
VideoRtpSession::deinitRecorder(std::shared_ptr<MediaRecorder>& rec)
{
    if (receiveThread_) {
        if (auto ob = rec->getStream(receiveThread_->getInfo().name)) {
            receiveThread_->detach(ob);
        }
    }
    if (auto input = std::static_pointer_cast<VideoInput>(videoLocal_)) {
        if (auto ob = rec->getStream(input->getInfo().name)) {
            input->detach(ob);
        }
    }
}

void
VideoRtpSession::setChangeOrientationCallback(std::function<void(int)> cb)
{
    changeOrientationCallback_ = std::move(cb);
}

}} // namespace jami::video
