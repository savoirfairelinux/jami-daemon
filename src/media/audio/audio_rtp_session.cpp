/*
 *  Copyright (C) 2014-2018 Savoir-faire Linux Inc.
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
#include "smartools.h"
#include <sstream>

namespace ring {

AudioRtpSession::AudioRtpSession(const std::string& id)
    : RtpSession(id)
{
    // don't move this into the initializer list or Cthulus will emerge
    ringbuffer_ = Manager::instance().getRingBufferPool().createRingBuffer(callID_);
}

AudioRtpSession::~AudioRtpSession()
{
    stop();
}

void
AudioRtpSession::startSender()
{
    if (not send_.enabled or send_.holding) {
        RING_WARN("Audio sending disabled");
        if (sender_) {
            if (socketPair_)
                socketPair_->interrupt();
            sender_.reset();
        }
        return;
    }

    if (sender_)
        RING_WARN("Restarting audio sender");

    // be sure to not send any packets before saving last RTP seq value
    socketPair_->stopSendOp();
    if (sender_)
        initSeqVal_ = sender_->getLastSeqValue() + 1;
    try {
        sender_.reset();
        socketPair_->stopSendOp(false);
        sender_.reset(new AudioSender(callID_, getRemoteRtpUri(), send_,
                                      *socketPair_, initSeqVal_, muteState_, mtu_));
    } catch (const MediaEncoderException &e) {
        RING_ERR("%s", e.what());
        send_.enabled = false;
    }
}

void
AudioRtpSession::restartSender()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // ensure that start has been called before restart
    if (not socketPair_)
        return;

    startSender();
}

void
AudioRtpSession::startReceiver()
{
    if (not receive_.enabled or receive_.holding) {
        RING_WARN("Audio receiving disabled");
        receiveThread_.reset();
        return;
    }

    if (receiveThread_)
        RING_WARN("Restarting audio receiver");

    auto accountAudioCodec = std::static_pointer_cast<AccountAudioCodecInfo>(receive_.codec);
    receiveThread_.reset(new AudioReceiveThread(callID_, accountAudioCodec->audioformat,
                                                receive_.receiving_sdp,
                                                mtu_));
    receiveThread_->addIOContext(*socketPair_);
    receiveThread_->startLoop();
}

void
AudioRtpSession::start(std::unique_ptr<IceSocket> rtp_sock, std::unique_ptr<IceSocket> rtcp_sock)
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
}

void
AudioRtpSession::stop()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (socketPair_)
        socketPair_->interrupt();

    receiveThread_.reset();
    sender_.reset();
    socketPair_.reset();
}
void
AudioRtpSession::setMuted(bool isMuted)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (sender_) {
        muteState_ = isMuted;
        sender_->setMuted(isMuted);
    }
}

void
AudioRtpSession::initRecorder(std::shared_ptr<MediaRecorder>& rec)
{
    if (receiveThread_)
        receiveThread_->initRecorder(rec);
    if (sender_)
        sender_->initRecorder(rec);
}

} // namespace ring
