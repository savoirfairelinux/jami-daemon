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

#include <sstream>
#include <map>
#include <string>

namespace ring { namespace video {

using std::map;
using std::string;

VideoRtpSession::VideoRtpSession(const string &callID, const DeviceParams& localVideoParams) :
    RtpSession(callID), localVideoParams_(localVideoParams)
{}

VideoRtpSession::~VideoRtpSession()
{ stop(); }

void VideoRtpSession::startSender()
{
    if (local_.enabled and not local_.holding) {
        if (sender_) {
            if (videoLocal_)
                videoLocal_->detach(sender_.get());
            if (videoMixer_)
                videoMixer_->detach(sender_.get());
            RING_WARN("Restarting video sender");
        }

        try {
            sender_.reset(new VideoSender(getRemoteRtpUri(), localVideoParams_, local_, *socketPair_));
        } catch (const MediaEncoderException &e) {
            RING_ERR("%s", e.what());
            local_.enabled = false;
        }
    }
}

void VideoRtpSession::startReceiver()
{
    if (remote_.enabled and not remote_.holding) {
        if (receiveThread_)
            RING_WARN("restarting video receiver");
        receiveThread_.reset(new VideoReceiveThread(callID_, remote_.receiving_sdp));
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

void VideoRtpSession::start()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (not local_.enabled and not remote_.enabled) {
        stop();
        return;
    }

    try {
        socketPair_.reset(new SocketPair(getRemoteRtpUri().c_str(), local_.addr.getPort()));
    } catch (const std::runtime_error &e) {
        RING_ERR("Socket creation failed on port %d: %s", local_.addr.getPort(), e.what());
        return;
    }

    startSender();
    startReceiver();

    // Setup video pipeline
    if (conference_)
        setupConferenceVideoPipeline(conference_);
    else if (sender_) {
        videoLocal_ = getVideoCamera();
        if (videoLocal_ and videoLocal_->attach(sender_.get()))
            DRing::switchToCamera();
    } else {
        videoLocal_.reset();
    }
}

void VideoRtpSession::start(std::unique_ptr<IceSocket> rtp_sock,
                            std::unique_ptr<IceSocket> rtcp_sock)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (not local_.enabled and not remote_.enabled) {
        stop();
        return;
    }

    socketPair_.reset(new SocketPair(std::move(rtp_sock), std::move(rtcp_sock)));

    startSender();
    startReceiver();

    // Setup video pipeline
    if (conference_)
        setupConferenceVideoPipeline(conference_);
    else if (sender_) {
        videoLocal_ = getVideoCamera();
        if (videoLocal_ and videoLocal_->attach(sender_.get()))
            DRing::switchToCamera();
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

    if (socketPair_)
        socketPair_->interrupt();

    receiveThread_.reset();
    sender_.reset();
    socketPair_.reset();
    videoLocal_.reset();
    conference_ = nullptr;
}

void VideoRtpSession::forceKeyFrame()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (sender_)
        sender_->forceKeyFrame();
}

void VideoRtpSession::setupConferenceVideoPipeline(Conference* conference)
{
    assert(conference);

    videoMixer_ = std::move(conference->getVideoMixer());
    assert(videoMixer_.get());
    videoMixer_->setDimensions(localVideoParams_.width, localVideoParams_.height);

    if (sender_) {
        // Swap sender from local video to conference video mixer
        if (videoLocal_)
            videoLocal_->detach(sender_.get());
        videoMixer_->attach(sender_.get());
    }

    if (receiveThread_) {
        receiveThread_->enterConference();
        receiveThread_->attach(videoMixer_.get());
    }
}

void VideoRtpSession::enterConference(Conference* conference)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    exitConference();

    conference_ = conference;

    if (local_.enabled or receiveThread_)
        setupConferenceVideoPipeline(conference);
}

void VideoRtpSession::exitConference()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);


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

}} // namespace ring::video
