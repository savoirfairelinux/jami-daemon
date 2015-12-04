/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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

#include "account_const.h"

#include <sstream>
#include <map>
#include <string>
#include <thread>
#include <chrono>

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

void
VideoRtpSession::startSender()
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
            if (auto input = Manager::instance().getVideoManager().videoInput.lock()) {
                auto newParams = input->switchInput(input_);
                try {
                    if (newParams.valid() &&
                        newParams.wait_for(NEWPARAMS_TIMEOUT) == std::future_status::ready)
                        localVideoParams_ = newParams.get();
                    else
                        RING_ERR("No valid new video parameters.");
                } catch (const std::exception& e) {
                    RING_ERR("Exception during retrieving video parameters: %s",
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
        auto codecVideo = std::static_pointer_cast<ring::AccountVideoCodecInfo>(send_.codec);
        auto isAutoQualityEnabledStr = codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::AUTO_QUALITY_ENABLED];
        if ((not rtcpCheckerThread_.isRunning()) && (isAutoQualityEnabledStr.compare(TRUE_STR) == 0))
            rtcpCheckerThread_.start();
        else if ((rtcpCheckerThread_.isRunning()) && (isAutoQualityEnabledStr.compare(FALSE_STR) == 0))
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

void
VideoRtpSession::startReceiver()
{
    if (receive_.enabled and not receive_.holding) {
        if (receiveThread_)
            RING_WARN("restarting video receiver");
        receiveThread_.reset(
            new VideoReceiveThread(callID_, receive_.receiving_sdp)
        );
        /* ebail: keyframe requests can lead to timeout if they are not answered.
         * we decided so to disable them for the moment
        receiveThread_->setRequestKeyFrameCallback(&SIPVoIPLink::enqueueKeyframeRequest);
        */
        receiveThread_->addIOContext(*socketPair_);
        receiveThread_->startLoop();
    } else {
        RING_DBG("Video receiving disabled");
        if (receiveThread_)
            receiveThread_->detach(videoMixer_.get());
        receiveThread_.reset();
    }
}
void
VideoRtpSession::start(std::unique_ptr<IceSocket> rtp_sock,
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

    setupVideoPipeline();
}

void
VideoRtpSession::stop()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    rtcpCheckerThread_.join();

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

void
VideoRtpSession::forceKeyFrame()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (sender_)
        sender_->forceKeyFrame();
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
    RING_DBG("[call:%s] Setup video pipeline on conference %s", callID_.c_str(),
             conference.getConfID().c_str());
    videoMixer_ = conference.getVideoMixer();

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

void
VideoRtpSession::enterConference(Conference* conference)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    exitConference();

    conference_ = conference;
    RING_DBG("[call:%s] enterConference (conf: %s)", callID_.c_str(),
             conference->getConfID().c_str());

    if (send_.enabled or receiveThread_) {
        videoMixer_ = conference->getVideoMixer();
        videoMixer_->setDimensions(localVideoParams_.width, localVideoParams_.height);
        setupConferenceVideoPipeline(*conference_);
    }
}

void
VideoRtpSession::exitConference()
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

bool
VideoRtpSession::increaseBitrate() {
    unsigned jump = 0;
    auto bitrateChanged = false;

    tableBitrate_[indexBitrate_].statSuccess++;

    //already reach highest bitrate
    if (indexBitrate_ == tableBitrate_.size() - 1)
        return bitrateChanged;

    auto distToMax = tableBitrate_.size() - 1 - indexBitrate_;

    if (distToMax <= (tableBitrate_.size() * 1/3))
        jump = std::min(indexBitrate_ + 1, (unsigned) (tableBitrate_.size() - 1));
    else if (distToMax <= (tableBitrate_.size() * 2/3))
        jump = std::min(indexBitrate_ + 2, (unsigned) (tableBitrate_.size() - 1));
    else
        jump = std::min(indexBitrate_ + 3, (unsigned) (tableBitrate_.size() - 1));

    while (indexBitrate_ < jump) {
        if (tableBitrate_[indexBitrate_].state == QUALITY_KO)
            break;

        tableBitrate_[indexBitrate_].state = QUALITY_OK;
        tableBitrate_[indexBitrate_].statTotal++;
        tableBitrate_[indexBitrate_].statSuccess++;
        indexBitrate_++;
        videoBitrateInfo_.videoBitrateCurrent = tableBitrate_[indexBitrate_].val;
        bitrateChanged = true;
    }

    return bitrateChanged;
}

bool
VideoRtpSession::decreaseBitrate(float packetLoss) {
    unsigned step = 0;

    tableBitrate_[indexBitrate_].statFailure++;

    //already reach lowest bitrate
    if (indexBitrate_ == 0)
        return false;

    if (packetLoss <= PACKET_LOSS_LOW)
        step = 1 ;
    else if (packetLoss <= PACKET_LOSS_MEDIUM)
        step = 2;
    else if (packetLoss <= PACKET_LOSS_HIGH)
        step = 3;
    else
        step = 4;

    unsigned jump = std::max((signed)(indexBitrate_ - step), (signed) 0);

    while ( indexBitrate_ > jump) {
        tableBitrate_[indexBitrate_].state = QUALITY_KO;
        tableBitrate_[indexBitrate_].statTotal++;
        tableBitrate_[indexBitrate_].statFailure++;
        indexBitrate_--;
        videoBitrateInfo_.videoBitrateCurrent = tableBitrate_[indexBitrate_].val;
    }
    return true;
}

bool
VideoRtpSession::increaseQuality() {
    unsigned jump = 0;
    auto qualityChanged = false;

    tableQuality_[indexQuality_].statSuccess++;

    //already reach highest quality
    if (indexQuality_ == tableQuality_.size() - 1)
        return qualityChanged;

    auto distToMax = tableQuality_.size() - 1 - indexQuality_;

    if (distToMax <= (tableQuality_.size() * 1/3))
        jump = std::min(indexQuality_ + 1, (unsigned) (tableQuality_.size() - 1));
    else if (distToMax <= (tableQuality_.size() * 2/3))
        jump = std::min(indexQuality_ + 2, (unsigned) (tableQuality_.size() - 1));
    else
        jump = std::min(indexQuality_ + 3, (unsigned) (tableQuality_.size() - 1));

    while (indexQuality_ < jump) {
        if (tableQuality_[indexQuality_].state == QUALITY_KO)
            break;

        tableQuality_[indexQuality_].state = QUALITY_OK;
        tableQuality_[indexQuality_].statTotal++;
        tableQuality_[indexQuality_].statSuccess++;
        indexQuality_++;
        videoBitrateInfo_.videoQualityCurrent = tableQuality_[indexQuality_].val;
        qualityChanged = true;
    }

    return qualityChanged;
}

bool
VideoRtpSession::decreaseQuality(float packetLoss) {
    unsigned step = 0;

    tableQuality_[indexQuality_].statFailure++;

    //already reach lowest quality
    if (indexQuality_ == 0)
        return false;

    if (packetLoss <= PACKET_LOSS_LOW)
        step = 1 ;
    else if (packetLoss <= PACKET_LOSS_MEDIUM)
        step = 2;
    else if (packetLoss <= PACKET_LOSS_HIGH)
        step = 3;
    else
        step = 4;

    unsigned jump = std::max((signed)(indexQuality_ - step), (signed) 0);

    while ( indexQuality_ > jump) {
        tableQuality_[indexQuality_].state = QUALITY_KO;
        tableQuality_[indexQuality_].statTotal++;
        tableQuality_[indexQuality_].statFailure++;
        indexQuality_--;
        videoBitrateInfo_.videoQualityCurrent = tableQuality_[indexQuality_].val;
    }
    return true;
}

void
VideoRtpSession::updateQualityTable() {
    //dumpStat();
    for (auto& qualityIt : tableQuality_) {
        if (qualityIt.state == QUALITY_KO) {
            //we consider that less than 2 failures are ok
            if (qualityIt.statFailure <= 2) {
                qualityIt.state = QUALITY_OK;
                RING_WARN("-> quality %u can be used (less than 2 failures) ",
                        qualityIt.val);
            } else if (( qualityIt.statTotal / qualityIt.statFailure) >= 3) {
                RING_WARN("-> quality %u can be used (ratio ok) ", qualityIt.val);
                qualityIt.state = QUALITY_OK;
            }

        }
    }
}

void
VideoRtpSession::updateBitrateTable() {
    for (auto& bitrateIt : tableBitrate_) {
        if (bitrateIt.state == QUALITY_KO) {
            //we consider that less than 2 failures are ok
            if (bitrateIt.statFailure <= 2) {
                bitrateIt.state = QUALITY_OK;
                RING_WARN("-> bitrate %u can be used (less than 2 failures) ",
                        bitrateIt.val);
            } else if (( bitrateIt.statTotal / bitrateIt.statFailure) >= 3) {
                RING_WARN("-> bitrate %u can be used (ratio ok) ", bitrateIt.val);
                bitrateIt.state = QUALITY_OK;
            }
        }
    }
}

void
VideoRtpSession::initBitrateAndQualityTable() {
    if (videoBitrateInfo_.videoQualityCurrent != SystemCodecInfo::DEFAULT_NO_QUALITY) {
        // create table of quality with a step of 1
        for (unsigned q = videoBitrateInfo_.videoQualityMin; q >= videoBitrateInfo_.videoQualityMax ; q--) {
            QualityInfo info = {q, QUALITY_UNTESTED, 0, 0, 0};
            tableQuality_.push_back(info);
        }
    }

    // create table of bitrate with a step of 50
    for (unsigned b = videoBitrateInfo_.videoBitrateMin; b <= videoBitrateInfo_.videoBitrateMax ; b+=50) {
        QualityInfo info = {b, QUALITY_UNTESTED, 0, 0, 0};
        tableBitrate_.push_back(info);
    }
}

void
VideoRtpSession::resetBitrateAndQualityStats() {
    for (auto& itQ : tableQuality_)
        itQ.statFailure = itQ.statSuccess = itQ.statTotal = 0;

    for (auto& itB : tableBitrate_)
        itB.statFailure = itB.statSuccess = itB.statTotal = 0;
}

void
VideoRtpSession::dumpStat() {
    for (const auto& qualityIt : tableQuality_) {
        RING_ERR("quality: %u; state %u; statTotal %u; statSuccess %u; statFailure %u",
                qualityIt.val,
                qualityIt.state,
                qualityIt.statTotal,
                qualityIt.statSuccess,
                qualityIt.statFailure);
    }

    for (const auto& bitrateIt : tableBitrate_) {
        RING_ERR("bitrate: %u; state %u; statTotal %u; statSuccess %u; statFailure %u",
                bitrateIt.val,
                bitrateIt.state,
                bitrateIt.statTotal,
                bitrateIt.statSuccess,
                bitrateIt.statFailure);
    }
}

void
VideoRtpSession::adaptQualityAndBitrate()
{
    bool mediaRestartNeeded = false;
    float packetLostRate = 0.0;

    auto rtcpCheckTimer = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now() - lastRTCPCheck_);
    auto rtcpLongCheckTimer = std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now() - lastLongRTCPCheck_);


    if (rtcpLongCheckTimer.count() >= RTCP_LONG_CHECKING_INTERVAL) {
        lastLongRTCPCheck_ = std::chrono::system_clock::now();
        updateBitrateTable();
        updateQualityTable();
        return;
    }

    if (rtcpCheckTimer.count() >= RTCP_CHECKING_INTERVAL) {
        lastRTCPCheck_ = std::chrono::system_clock::now();

        if (videoBitrateInfo_.videoQualityCurrent != SystemCodecInfo::DEFAULT_NO_QUALITY)
            tableQuality_[indexQuality_].statTotal++;

        tableBitrate_[indexBitrate_].statTotal++;

        //packetLostRate is not already available. Do nothing
        if ((packetLostRate = checkPeerPacketLoss()) == NO_PACKET_LOSS_CALCULATED) {
            if (videoBitrateInfo_.videoQualityCurrent != SystemCodecInfo::DEFAULT_NO_QUALITY)
                tableQuality_[indexQuality_].statSuccess++;

            tableBitrate_[indexBitrate_].statSuccess++;

        //too much packet lost : decrease quality and bitrate
        } else if (packetLostRate >= videoBitrateInfo_.packetLostThreshold) {

            //decrease quality and bitrate. Quality has the priority
            mediaRestartNeeded = decreaseBitrate(packetLostRate);
            if (videoBitrateInfo_.videoQualityCurrent != SystemCodecInfo::DEFAULT_NO_QUALITY)
                mediaRestartNeeded = decreaseQuality(packetLostRate);

        //no packet lost: increase quality and bitrate
        } else {
            //increase quality and bitrate. Quality has the priority
            mediaRestartNeeded = increaseBitrate();
            if (videoBitrateInfo_.videoQualityCurrent != SystemCodecInfo::DEFAULT_NO_QUALITY)
                mediaRestartNeeded = increaseQuality();
        }
    }

    if (mediaRestartNeeded) {
        storeVideoBitrateInfo();
        const auto& cid = callID_;

        RING_WARN("packetLostRate=%f : quality->%d | bitrate->%d",
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
VideoRtpSession::getVideoBitrateInfo() {
    auto codecVideo = std::static_pointer_cast<ring::AccountVideoCodecInfo>(send_.codec);
    if (codecVideo) {
        videoBitrateInfo_ = {
            (unsigned)(ring::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::BITRATE])),
            (unsigned)(ring::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE])),
            (unsigned)(ring::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE])),
            (unsigned)(ring::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::QUALITY])),
            (unsigned)(ring::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::MIN_QUALITY])),
            (unsigned)(ring::stoi(codecVideo->getCodecSpecifications()[DRing::Account::ConfProperties::CodecInfo::MAX_QUALITY])),
            videoBitrateInfo_.packetLostThreshold,
        };
    } else {
        videoBitrateInfo_ = {0,0,0,0,0,0,0};
    }
}

void
VideoRtpSession::storeVideoBitrateInfo() {
    auto codecVideo = std::static_pointer_cast<ring::AccountVideoCodecInfo>(send_.codec);

    if (codecVideo) {
        codecVideo->setCodecSpecifications({
            {DRing::Account::ConfProperties::CodecInfo::BITRATE, ring::to_string(videoBitrateInfo_.videoBitrateCurrent)},
            {DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE, ring::to_string(videoBitrateInfo_.videoBitrateMin)},
            {DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE, ring::to_string(videoBitrateInfo_.videoBitrateMax)},
            {DRing::Account::ConfProperties::CodecInfo::QUALITY, ring::to_string(videoBitrateInfo_.videoQualityCurrent)},
            {DRing::Account::ConfProperties::CodecInfo::MIN_QUALITY, ring::to_string(videoBitrateInfo_.videoQualityMin)},
            {DRing::Account::ConfProperties::CodecInfo::MAX_QUALITY, ring::to_string(videoBitrateInfo_.videoQualityMax)}
        });

    }
}

bool
VideoRtpSession::setupRtcpChecker()
{
    getVideoBitrateInfo();
    if (tableQuality_.empty())
        initBitrateAndQualityTable();
    else
        resetBitrateAndQualityStats();

    // we move to the midle of the vector
    indexQuality_ = tableQuality_.size() / 2;
    indexBitrate_ = tableBitrate_.size() / 2;

    if (videoBitrateInfo_.videoQualityCurrent != SystemCodecInfo::DEFAULT_NO_QUALITY)
        videoBitrateInfo_.videoQualityCurrent = tableQuality_[indexQuality_].val;

    videoBitrateInfo_.videoBitrateCurrent = tableBitrate_[indexBitrate_].val;
    return true;
}

void
VideoRtpSession::processRtcpChecker()
{
    adaptQualityAndBitrate();
    rtcpCheckerThread_.wait_for(std::chrono::seconds(RTCP_CHECKING_INTERVAL));
}

void
VideoRtpSession::cleanupRtcpChecker()
{}

}} // namespace ring::video
