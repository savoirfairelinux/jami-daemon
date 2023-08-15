/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "audio_rtp_session.h"

#include "logger.h"
#include "noncopyable.h"
#include "sip/sdp.h"

#include "audio_receive_thread.h"
#include "audio_sender.h"
#include "socket_pair.h"
#include "media_recorder.h"
#include "media_encoder.h"
#include "media_decoder.h"
#include "media_io_handle.h"
#include "media_device.h"

#include "audio/audio_input.h"
#include "audio/audiobuffer.h"
#include "audio/ringbufferpool.h"
#include "audio/resampler.h"
#include "client/videomanager.h"
#include "manager.h"
#include "observer.h"
#include <sstream>

namespace jami {

AudioRtpSession::AudioRtpSession(const std::string& callId,
                                 const std::string& streamId,
                                 const std::shared_ptr<MediaRecorder>& rec)
    : RtpSession(callId, streamId, MediaType::MEDIA_AUDIO)
    , rtcpCheckerThread_([] { return true; }, [this] { processRtcpChecker(); }, [] {})

{
    recorder_ = rec;
    JAMI_DBG("Created Audio RTP session: %p - call Id %s", this, callId_.c_str());

    // don't move this into the initializer list or Cthulus will emerge
    ringbuffer_ = Manager::instance().getRingBufferPool().createRingBuffer(streamId_);
}

AudioRtpSession::~AudioRtpSession()
{
    deinitRecorder();
    stop();
    JAMI_DBG("Destroyed Audio RTP session: %p - call Id %s", this, callId_.c_str());
}

void
AudioRtpSession::startSender()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    JAMI_DBG("Start audio RTP sender: input [%s] - muted [%s]",
             input_.c_str(),
             muteState_ ? "YES" : "NO");

    if (not send_.enabled or send_.onHold) {
        JAMI_WARN("Audio sending disabled");
        if (sender_) {
            if (socketPair_)
                socketPair_->interrupt();
            if (audioInput_)
                audioInput_->detach(sender_.get());
            sender_.reset();
        }
        return;
    }

    if (sender_)
        JAMI_WARN("Restarting audio sender");
    if (audioInput_)
        audioInput_->detach(sender_.get());

    // sender sets up input correctly, we just keep a reference in case startSender is called
    audioInput_ = jami::getAudioInput(streamId_);
    audioInput_->setRecorderCallback([this](const MediaStream& ms) { attachLocalRecorder(ms); });
    audioInput_->setMuted(muteState_);
    audioInput_->setSuccessfulSetupCb(onSuccessfulSetup_);
    auto newParams = audioInput_->switchInput(input_);
    try {
        if (newParams.valid()
            && newParams.wait_for(NEWPARAMS_TIMEOUT) == std::future_status::ready) {
            localAudioParams_ = newParams.get();
        } else {
            JAMI_ERR() << "No valid new audio parameters";
            return;
        }
    } catch (const std::exception& e) {
        JAMI_ERR() << "Exception while retrieving audio parameters: " << e.what();
        return;
    }

    send_.fecEnabled = true;

    // be sure to not send any packets before saving last RTP seq value
    socketPair_->stopSendOp();
    if (sender_)
        initSeqVal_ = sender_->getLastSeqValue() + 1;
    try {
        sender_.reset();
        socketPair_->stopSendOp(false);
        sender_.reset(new AudioSender(getRemoteRtpUri(), send_, *socketPair_, initSeqVal_, mtu_));
    } catch (const MediaEncoderException& e) {
        JAMI_ERR("%s", e.what());
        send_.enabled = false;
    }

    if (voiceCallback_) {
        sender_->setVoiceCallback(voiceCallback_);
    }

    // NOTE do after sender/encoder are ready
    auto codec = std::static_pointer_cast<SystemAudioCodecInfo>(send_.codec);
    audioInput_->setFormat(codec->audioformat);
    if (audioInput_)
        audioInput_->attach(sender_.get());

    if (not rtcpCheckerThread_.isRunning())
        rtcpCheckerThread_.start();
}

void
AudioRtpSession::restartSender()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // ensure that start has been called before restart
    if (not socketPair_) {
        return;
    }

    startSender();
}

void
AudioRtpSession::startReceiver()
{
    if (socketPair_)
        socketPair_->setReadBlockingMode(true);

    if (not receive_.enabled or receive_.onHold) {
        JAMI_WARN("Audio receiving disabled");
        receiveThread_.reset();
        return;
    }

    if (receiveThread_)
        JAMI_WARN("Restarting audio receiver");

    auto accountAudioCodec = std::static_pointer_cast<SystemAudioCodecInfo>(receive_.codec);
    receiveThread_.reset(new AudioReceiveThread(streamId_,
                                                accountAudioCodec->audioformat,
                                                receive_.receiving_sdp,
                                                mtu_));

    receiveThread_->setRecorderCallback([this](const MediaStream& ms) { attachRemoteRecorder(ms); });
    receiveThread_->addIOContext(*socketPair_);
    receiveThread_->setSuccessfulSetupCb(onSuccessfulSetup_);
    receiveThread_->startReceiver();
}

void
AudioRtpSession::start(std::unique_ptr<dhtnet::IceSocket> rtp_sock, std::unique_ptr<dhtnet::IceSocket> rtcp_sock)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (not send_.enabled and not receive_.enabled) {
        stop();
        return;
    }

    try {
        if (rtp_sock and rtcp_sock) {
            if (send_.addr) {
                rtp_sock->setDefaultRemoteAddress(send_.addr);
            }

            auto& rtcpAddr = send_.rtcp_addr ? send_.rtcp_addr : send_.addr;
            if (rtcpAddr) {
                rtcp_sock->setDefaultRemoteAddress(rtcpAddr);
            }

            socketPair_.reset(new SocketPair(std::move(rtp_sock), std::move(rtcp_sock)));
        } else {
            socketPair_.reset(new SocketPair(getRemoteRtpUri().c_str(), receive_.addr.getPort()));
        }

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
}

void
AudioRtpSession::stop()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    JAMI_DBG("[%p] Stopping receiver", this);

    if (not receiveThread_)
        return;

    if (socketPair_)
        socketPair_->setReadBlockingMode(false);

    receiveThread_->stopReceiver();

    if (audioInput_)
        audioInput_->detach(sender_.get());

    if (socketPair_)
        socketPair_->interrupt();

    rtcpCheckerThread_.join();

    receiveThread_.reset();
    sender_.reset();
    socketPair_.reset();
    audioInput_.reset();
}

void
AudioRtpSession::setMuted(bool muted, Direction dir)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (dir == Direction::SEND) {
        muteState_ = muted;
        if (audioInput_)
            audioInput_->setMuted(muted);
    } else {
        if (receiveThread_) {
            auto ms = receiveThread_->getInfo();
            if (muted) {
                if (auto ob = recorder_->getStream(ms.name)) {
                    receiveThread_->detach(ob);
                    recorder_->removeStream(ms);
                }
            } else {
                if (auto ob = recorder_->addStream(ms)) {
                    receiveThread_->attach(ob);
                }
            }
        }
    }
}

void
AudioRtpSession::setVoiceCallback(std::function<void(bool)> cb)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    voiceCallback_ = std::move(cb);
    if (sender_) {
        sender_->setVoiceCallback(voiceCallback_);
    }
}

bool
AudioRtpSession::check_RCTP_Info_RR(RTCPInfo& rtcpi)
{
    auto rtcpInfoVect = socketPair_->getRtcpRR();
    unsigned totalLost = 0;
    unsigned totalJitter = 0;
    unsigned nbDropNotNull = 0;
    auto vectSize = rtcpInfoVect.size();

    if (vectSize != 0) {
        for (const auto& it : rtcpInfoVect) {
            if (it.fraction_lost != 0) // Exclude null drop
                nbDropNotNull++;
            totalLost += it.fraction_lost;
            totalJitter += ntohl(it.jitter);
        }
        rtcpi.packetLoss = nbDropNotNull ? (float) (100 * totalLost) / (256.0 * nbDropNotNull) : 0;
        // Jitter is expressed in timestamp unit -> convert to milliseconds
        // https://stackoverflow.com/questions/51956520/convert-jitter-from-rtp-timestamp-unit-to-millisseconds
        rtcpi.jitter = (totalJitter / vectSize / 90000.0f) * 1000;
        rtcpi.nb_sample = vectSize;
        rtcpi.latency = socketPair_->getLastLatency();
        return true;
    }
    return false;
}

void
AudioRtpSession::adaptQualityAndBitrate()
{
    RTCPInfo rtcpi {};
    if (check_RCTP_Info_RR(rtcpi)) {
        dropProcessing(&rtcpi);
    }
}

void
AudioRtpSession::dropProcessing(RTCPInfo* rtcpi)
{
    auto pondLoss = getPonderateLoss(rtcpi->packetLoss);
    setNewPacketLoss(pondLoss);
}

void
AudioRtpSession::setNewPacketLoss(unsigned int newPL)
{
    newPL = std::clamp((int) newPL, 0, 100);
    if (newPL != packetLoss_) {
        if (sender_) {
            auto ret = sender_->setPacketLoss(newPL);
            packetLoss_ = newPL;
            if (ret == -1)
                JAMI_ERR("Fail to access the encoder");
        } else {
            JAMI_ERR("Fail to access the sender");
        }
    }
}

float
AudioRtpSession::getPonderateLoss(float lastLoss)
{
    static float pond = 10.0f;

    pond = floor(0.5 * lastLoss + 0.5 * pond);
    if (lastLoss > pond) {
        return lastLoss;
    } else {
        return pond;
    }
}

void
AudioRtpSession::processRtcpChecker()
{
    adaptQualityAndBitrate();
    socketPair_->waitForRTCP(std::chrono::seconds(rtcp_checking_interval));
}

void
AudioRtpSession::attachRemoteRecorder(const MediaStream& ms)
{
    if (!recorder_ || !receiveThread_)
        return;
    if (auto ob = recorder_->addStream(ms)) {
        receiveThread_->attach(ob);
    }
}

void
AudioRtpSession::attachLocalRecorder(const MediaStream& ms)
{
    if (!recorder_ || !audioInput_)
        return;
    if (auto ob = recorder_->addStream(ms)) {
        audioInput_->attach(ob);
    }
}

void
AudioRtpSession::initRecorder()
{
    if (!recorder_)
        return;
    if (receiveThread_)
        receiveThread_->setRecorderCallback(
            [this](const MediaStream& ms) { attachRemoteRecorder(ms); });
    if (audioInput_)
        audioInput_->setRecorderCallback(
            [this](const MediaStream& ms) { attachLocalRecorder(ms); });
}

void
AudioRtpSession::deinitRecorder()
{
    if (!recorder_)
        return;
    if (receiveThread_) {
        auto ms = receiveThread_->getInfo();
        if (auto ob = recorder_->getStream(ms.name)) {
            receiveThread_->detach(ob);
            recorder_->removeStream(ms);
        }
    }
    if (audioInput_) {
        auto ms = audioInput_->getInfo();
        if (auto ob = recorder_->getStream(ms.name)) {
            audioInput_->detach(ob);
            recorder_->removeStream(ms);
        }
    }
}

} // namespace jami
