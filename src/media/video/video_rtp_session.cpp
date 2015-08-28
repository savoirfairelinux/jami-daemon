/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
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

#include "account_const.h"

#include <sstream>
#include <map>
#include <string>
#include <thread>

namespace ring { namespace video {

using std::map;
using std::string;

constexpr static auto NEWPARAMS_TIMEOUT = std::chrono::milliseconds(1000);

VideoRtpSession::VideoRtpSession(const string &callID,
                                 const DeviceParams& localVideoParams) :
    RtpSession(callID), localVideoParams_(localVideoParams)
    , lastRTCPCheck_(std::chrono::system_clock::now())
    , lastLongRTCPCheck_(std::chrono::system_clock::now())
    , rtcpCheckerThread_(std::bind(&VideoRtpSession::setupRtcpChecker, this),
            std::bind(&VideoRtpSession::processRtcpChecker, this),
            std::bind(&VideoRtpSession::cleanupRtcpChecker, this))
{}

VideoRtpSession::~VideoRtpSession()
{ stop(); }

void VideoRtpSession::startSender()
{
    if (send_.enabled and not send_.holding) {
        if (sender_) {
            if (videoLocal_)
                videoLocal_->detach(sender_.get());
            if (videoMixer_)
                videoMixer_->detach(sender_.get());
            RING_WARN("Restarting video sender");
        }

        if (not conference_) {
            videoLocal_ = getVideoCamera();
            if (auto input = videoManager.videoInput.lock()) {
                auto newParams = input->switchInput(input_);
                try {
                    if (newParams.valid() &&
                        newParams.wait_for(NEWPARAMS_TIMEOUT) == std::future_status::ready)
                        localVideoParams_ = newParams.get();
                    else
                        RING_ERR("No valid new video parameters.");
                } catch (const std::exception& e) {
                    RING_ERR("Exception during retriving video parameters: %s",
                             e.what());
                }
            } else {
                RING_WARN("Can't lock video input");
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
                                          send_, *socketPair_, initSeqVal_));
        } catch (const MediaEncoderException &e) {
            RING_ERR("%s", e.what());
            send_.enabled = false;
        }
        if (not rtcpCheckerThread_.isRunning())
            rtcpCheckerThread_.start();
    }
}

void
VideoRtpSession::restartSender()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    startSender();

    if (sender_) {
        if (videoLocal_)
            videoLocal_->attach(sender_.get());
    }
}

void VideoRtpSession::startReceiver()
{
    if (receive_.enabled and not receive_.holding) {
        if (receiveThread_)
            RING_WARN("restarting video receiver");
        receiveThread_.reset(
            new VideoReceiveThread(callID_, receive_.receiving_sdp)
        );
        receiveThread_->setRequestKeyFrameCallback(&SIPVoIPLink::enqueueKeyframeRequest);
        receiveThread_->addIOContext(*socketPair_);
        receiveThread_->startLoop();
    } else {
        RING_DBG("Video receiving disabled");
        if (receiveThread_)
            receiveThread_->detach(videoMixer_.get());
        receiveThread_.reset();
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
        RING_ERR("Socket creation failed: %s", e.what());
        return;
    }

    startSender();
    startReceiver();

    // Setup video pipeline
    if (conference_)
        setupConferenceVideoPipeline(*conference_);
    else if (sender_) {
        if (videoLocal_)
            videoLocal_->attach(sender_.get());
    } else {
        videoLocal_.reset();
    }
}

void VideoRtpSession::stop()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (videoLocal_)
        videoLocal_->detach(sender_.get());

    if (videoMixer_) {
        videoMixer_->detach(sender_.get());
        if (receiveThread_)
            receiveThread_->detach(videoMixer_.get());
    }

    rtcpCheckerThread_.stop();

    if (socketPair_)
        socketPair_->interrupt();

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

void VideoRtpSession::setupConferenceVideoPipeline(Conference& conference)
{
    RING_DBG("[call:%s] Setup video pipeline on conference %s", callID_.c_str(),
             conference.getConfID().c_str());
    videoMixer_ = conference.getVideoMixer();
    videoMixer_->setDimensions(localVideoParams_.width, localVideoParams_.height);

    if (sender_) {
        // Swap sender from local video to conference video mixer
        if (videoLocal_)
            videoLocal_->detach(sender_.get());
        videoMixer_->attach(sender_.get());
    } else
        RING_WARN("[call:%s] no sender", callID_.c_str());

    if (receiveThread_) {
        receiveThread_->enterConference();
        receiveThread_->attach(videoMixer_.get());
    } else
        RING_WARN("[call:%s] no receiver", callID_.c_str());
}

void VideoRtpSession::enterConference(Conference* conference)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    exitConference();

    conference_ = conference;
    RING_DBG("[call:%s] enterConference (conf: %s)", callID_.c_str(),
             conference->getConfID().c_str());

    if (send_.enabled or receiveThread_)
        setupConferenceVideoPipeline(*conference_);
}

void VideoRtpSession::exitConference()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!conference_)
        return;

    RING_DBG("[call:%s] exitConference (conf: %s)", callID_.c_str(),
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
VideoRtpSession::useCodec(const ring::AccountVideoCodecInfo* codec) const
{
    return sender_->useCodec(codec);
}


float
VideoRtpSession::checkPeerPacketLoss()
{
    auto rtcpInfoVect = socketPair_->getRtcpInfo();
    auto totalLost = 0;
    auto fract = 0;
    auto vectSize = rtcpInfoVect.size();

    for (auto it : rtcpInfoVect) {
        fract = (ntohl(it.fraction_lost) & 0xff000000)  >> 24;
        totalLost += fract;
    }

    if (vectSize != 0)
        return (float)( 100 * totalLost) / (float)(256.0 * vectSize);
    else
        return NO_PACKET_LOSS_CALCULATED;
}

static void
restartMediaEncoder(std::shared_ptr<Call> call)
{
    if (call)
        call->restartMediaSender();
    else
        RING_ERR("can not restart media encoder: call is null !");
}

void
VideoRtpSession::adaptBitrate()
{
    bool needToCheckBitrate = false;
    float packetLostRate = 0.0;

    auto rtcpCheckTimer = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now() - lastRTCPCheck_);
    auto rtcpLongCheckTimer = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now() - lastLongRTCPCheck_);

    if (rtcpCheckTimer.count() >= RTCP_CHECKING_INTERVAL) {
        needToCheckBitrate = true;
        lastRTCPCheck_ = std::chrono::system_clock::now();
    } else if (rtcpLongCheckTimer.count() >= RTCP_LONG_CHECKING_INTERVAL) {
        RING_DBG("checking bitrate each %d seconds", RTCP_LONG_CHECKING_INTERVAL);
        needToCheckBitrate = true;
        lastLongRTCPCheck_ = std::chrono::system_clock::now();
        //we force iterative bitrate adaptation
        videoBitrateInfo_.cptBitrateChecking = 0;
    }


    if (needToCheckBitrate) {
        videoBitrateInfo_.cptBitrateChecking++;
        auto oldBitrate = videoBitrateInfo_.videoBitrateCurrent;

        //packetLostRate is not already available. Do nothing
        if ((packetLostRate = checkPeerPacketLoss()) == NO_PACKET_LOSS_CALCULATED) {
            //we force iterative bitrate adaptation
            videoBitrateInfo_.cptBitrateChecking = 0;

        //too much packet lost : decrease bitrate
        } else if (packetLostRate >= videoBitrateInfo_.packetLostThreshold) {

            //calculate new bitrate by dichotomie
            videoBitrateInfo_.videoBitrateCurrent =
                (videoBitrateInfo_.videoBitrateCurrent + videoBitrateInfo_.videoBitrateMin) / 2;

            //boundaries low
            if (videoBitrateInfo_.videoBitrateCurrent < videoBitrateInfo_.videoBitrateMin)
                videoBitrateInfo_.videoBitrateCurrent = videoBitrateInfo_.videoBitrateMin;

            RING_WARN("packetLostRate=%f >= %f -> decrease bitrate to %d",
                    packetLostRate,
                    videoBitrateInfo_.packetLostThreshold,
                    videoBitrateInfo_.videoBitrateCurrent);

            //we force iterative bitrate adaptation
            videoBitrateInfo_.cptBitrateChecking = 0;

            //asynchronous A/V media restart
            if (videoBitrateInfo_.videoBitrateCurrent != oldBitrate) {
                storeVideoBitrateInfo();
                runOnMainThread(std::bind(restartMediaEncoder, Manager::instance().callFactory.getCall(callID_)));
            }

        //no packet lost: increase bitrate
        } else if (videoBitrateInfo_.cptBitrateChecking <= videoBitrateInfo_.maxBitrateChecking) {

            //calculate new bitrate by dichotomie
            videoBitrateInfo_.videoBitrateCurrent =
                ( videoBitrateInfo_.videoBitrateCurrent + videoBitrateInfo_.videoBitrateMax) / 2;

            //boundaries high
            if (videoBitrateInfo_.videoBitrateCurrent > videoBitrateInfo_.videoBitrateMax)
                videoBitrateInfo_.videoBitrateCurrent = videoBitrateInfo_.videoBitrateMax;

            RING_WARN("[%u/%u] packetLostRate=%f < %f -> try to increase bitrate to %d",
                    videoBitrateInfo_.cptBitrateChecking,
                    videoBitrateInfo_.maxBitrateChecking,
                    packetLostRate,
                    videoBitrateInfo_.packetLostThreshold,
                    videoBitrateInfo_.videoBitrateCurrent);


            //asynchronous A/V media restart
            if (videoBitrateInfo_.videoBitrateCurrent != oldBitrate) {
                storeVideoBitrateInfo();
                runOnMainThread(std::bind(restartMediaEncoder,  Manager::instance().callFactory.getCall(callID_)));
            }

        } else {
            //nothing we reach maximal tries
        }
    }
}

void
VideoRtpSession::getVideoBitrateInfo() {
    auto codecVideo =
        std::static_pointer_cast<ring::AccountVideoCodecInfo>(send_.codec);
    if (codecVideo) {
        videoBitrateInfo_ = {
            (unsigned)(std::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::BITRATE])),
            (unsigned)(std::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE])),
            (unsigned)(std::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE])),
            videoBitrateInfo_.cptBitrateChecking,
            videoBitrateInfo_.maxBitrateChecking,
            videoBitrateInfo_.packetLostThreshold,
        };
    } else {
        videoBitrateInfo_ = {0,0,0,0,0,0};
    }
}

void
VideoRtpSession::storeVideoBitrateInfo() {
    auto codecVideo =
        std::static_pointer_cast<ring::AccountVideoCodecInfo>(send_.codec);

    if (codecVideo) {
        codecVideo->setCodecSpecifications(
            {
            {DRing::Account::ConfProperties::CodecInfo::BITRATE, ring::to_string(videoBitrateInfo_.videoBitrateCurrent)},
            {DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE, ring::to_string(videoBitrateInfo_.videoBitrateMin)},
            {DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE, ring::to_string(videoBitrateInfo_.videoBitrateMax)}
            });
    }
}
bool
VideoRtpSession::setupRtcpChecker()
{
    getVideoBitrateInfo();
    return true;
}

void
VideoRtpSession::processRtcpChecker()
{
    adaptBitrate();
    std::this_thread::sleep_for(std::chrono::seconds(RTCP_CHECKING_INTERVAL));
}

void
VideoRtpSession::cleanupRtcpChecker()
{}

}} // namespace ring::video
