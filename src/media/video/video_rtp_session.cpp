/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
#include "congestion_control.h"

#include "account_const.h"

#include <sstream>
#include <map>
#include <string>
#include <thread>
#include <chrono>

namespace jami { namespace video {

using std::string;

static constexpr unsigned MAX_SIZE_HISTO_QUALITY {30};
static constexpr unsigned MAX_SIZE_HISTO_BITRATE {100};
static constexpr unsigned MAX_SIZE_HISTO_JITTER {50};
static constexpr unsigned MAX_SIZE_HISTO_DELAY {25};


VideoRtpSession::VideoRtpSession(const string &callID,
                                 const DeviceParams& localVideoParams) :
    RtpSession(callID)
    , localVideoParams_(localVideoParams)
    , videoBitrateInfo_  {}
{}

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
    cbKeyFrameRequest_ = std::move(cb);
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

        auto codecVideo = std::static_pointer_cast<jami::AccountVideoCodecInfo>(send_.codec);
        auto autoQuality = codecVideo->isAutoQualityEnabled;

        send_.mode = autoQuality ? RateMode::CBR : RateMode::CRF_CONSTRAINED;
        send_.linkableHW = conference_ == nullptr;
        send_.bitrate = videoBitrateInfo_.videoBitrateCurrent;

        if (socketPair_)
            initSeqVal_ = socketPair_->lastSeqValOut();

        try {
            sender_.reset();
            socketPair_->stopSendOp(false);
            sender_.reset(new VideoSender(getRemoteRtpUri(), localVideoParams_,
                                          send_, *socketPair_, initSeqVal_+1, mtu_));
            if (changeOrientationCallback_)
                sender_->setChangeOrientationCallback(changeOrientationCallback_);
            if (socketPair_)
                socketPair_->setPacketLossCallback([this] (){
                cbKeyFrameRequest_();});

        } catch (const MediaEncoderException &e) {
            JAMI_ERR("%s", e.what());
            send_.enabled = false;
        }
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
        receiveThread_->addIOContext(*socketPair_);
        receiveThread_->startLoop(onSuccessfulSetup_);
        if (receiveThread_)
            receiveThread_->setRequestKeyFrameCallback([this] (){
                cbKeyFrameRequest_();});
    } else {
        JAMI_DBG("Video receiving disabled");
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
    if (videoLocal_)
        videoLocal_->detach(sender_.get());

    if (videoMixer_) {
        videoMixer_->detach(sender_.get());
        if (receiveThread_)
            receiveThread_->detach(videoMixer_.get());
    }

    if (socketPair_)
        socketPair_->interrupt();

    rtcpCheckerThread_.join();

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
    if (receiveThread_)
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
#if defined(__APPLE__) && TARGET_OS_MAC
        videoMixer_->setParameters(localVideoParams_.width,
                                   localVideoParams_.height,
                                   av_get_pix_fmt(localVideoParams_.pixel_format.c_str()));
#else
        videoMixer_->setParameters(localVideoParams_.width, localVideoParams_.height);
#endif
        setupConferenceVideoPipeline(*conference_);

        // Restart encoder with conference parameter ON in order to unlink HW encoder
        // from HW decoder.
        restartSender();
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

    // videoLocal_ is reset when a conference is created (only mixer need videoInput)
    // when the conference is removed, we need to set videoLocal_ for the remaining call
    if (!videoLocal_)
        videoLocal_ = getVideoCamera();

    if (videoLocal_)
        videoLocal_->attach(sender_.get());

    conference_ = nullptr;
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
VideoRtpSession::setNewBitrate(unsigned int newBR)
{
    newBR = std::max(newBR, videoBitrateInfo_.videoBitrateMin);
    newBR = std::min(newBR, videoBitrateInfo_.videoBitrateMax);

    if (videoBitrateInfo_.videoBitrateCurrent != newBR) {
        videoBitrateInfo_.videoBitrateCurrent = newBR;
        storeVideoBitrateInfo();

#if __ANDROID__
        if (auto input_device = std::dynamic_pointer_cast<VideoInput>(videoLocal_))
            emitSignal<DRing::VideoSignal::SetBitrate>(input_device->getParams().name, (int)newBR);
#endif

        if (sender_) {
            auto ret = sender_->setBitrate(newBR);
            if (ret == -1)
                JAMI_ERR("Fail to access the encoder");
            else if (ret == 0)
                restartSender();
        } else {
            JAMI_ERR("Fail to access the sender");
        }
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
    if (auto codecVideo = std::static_pointer_cast<jami::AccountVideoCodecInfo>(send_.codec)) {
        codecVideo->setCodecSpecifications({
            {DRing::Account::ConfProperties::CodecInfo::BITRATE, std::to_string(videoBitrateInfo_.videoBitrateCurrent)},
            {DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE, std::to_string(videoBitrateInfo_.videoBitrateMin)},
            {DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE, std::to_string(videoBitrateInfo_.videoBitrateMax)},
            {DRing::Account::ConfProperties::CodecInfo::QUALITY, std::to_string(videoBitrateInfo_.videoQualityCurrent)},
            {DRing::Account::ConfProperties::CodecInfo::MIN_QUALITY, std::to_string(videoBitrateInfo_.videoQualityMin)},
            {DRing::Account::ConfProperties::CodecInfo::MAX_QUALITY, std::to_string(videoBitrateInfo_.videoQualityMax)}
        });
    }

    if (histoQuality_.size() > MAX_SIZE_HISTO_QUALITY)
        histoQuality_.pop_front();

    if (histoBitrate_.size() > MAX_SIZE_HISTO_BITRATE)
        histoBitrate_.pop_front();

    histoQuality_.push_back(videoBitrateInfo_.videoQualityCurrent);
    histoBitrate_.push_back(videoBitrateInfo_.videoBitrateCurrent);
}

void
VideoRtpSession::processRtcpChecker()
{
    adaptQualityAndBitrate();
    socketPair_->waitForRTCP(std::chrono::seconds(rtcp_checking_interval));
}

void
VideoRtpSession::initRecorder(std::shared_ptr<MediaRecorder>& rec)
{
    if (receiveThread_) {
        if (auto ob = rec->addStream(receiveThread_->getInfo())) {
            receiveThread_->attach(ob);
        }
    }
    if (Manager::instance().videoPreferences.getRecordPreview()) {
        if (auto input = std::static_pointer_cast<VideoInput>(videoLocal_)) {
            if (auto ob = rec->addStream(input->getInfo())) {
                input->attach(ob);
            }
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
    if (sender_)
        sender_->setChangeOrientationCallback(changeOrientationCallback_);
}

}} // namespace jami::video
