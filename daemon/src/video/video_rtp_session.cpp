/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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
#include "socket_pair.h"
#include "sip/sdp.h"
#include "sip/sipvoiplink.h"
#include "manager.h"
#include "logger.h"

#include <sstream>
#include <map>
#include <string>

namespace sfl_video {

using std::map;
using std::string;

VideoRtpSession::VideoRtpSession(const string &callID,
								 const map<string, string> &txArgs) :
    txArgs_(txArgs), callID_(callID)
{}

VideoRtpSession::~VideoRtpSession()
{ stop(); }

void VideoRtpSession::updateSDP(const Sdp &sdp)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    string desc(sdp.getIncomingVideoDescription());
    // if port has changed
    if (not desc.empty() and desc != rxArgs_["receiving_sdp"]) {
        rxArgs_["receiving_sdp"] = desc;
        DEBUG("Updated incoming SDP to:\n%s",
              rxArgs_["receiving_sdp"].c_str());
    }

    if (desc.empty()) {
        DEBUG("Video is inactive");
        receiving_ = false;
        sending_ = false;
    } else if (desc.find("sendrecv") != string::npos) {
        DEBUG("Sending and receiving video");
        receiving_ = true;
        sending_ = true;
    } else if (desc.find("inactive") != string::npos) {
        DEBUG("Video is inactive");
        receiving_ = false;
        sending_ = false;
    } else if (desc.find("sendonly") != string::npos) {
        DEBUG("Receiving video disabled, video set to sendonly");
        receiving_ = false;
        sending_ = true;
    } else if (desc.find("recvonly") != string::npos) {
        DEBUG("Sending video disabled, video set to recvonly");
        sending_ = false;
        receiving_ = true;
    }
    // even if it says sendrecv or recvonly, our peer may disable video by
    // setting the port to 0
    if (desc.find("m=video 0") != string::npos) {
        DEBUG("Receiving video disabled, port was set to 0");
        receiving_ = false;
    }

    if (sending_)
        sending_ = sdp.getOutgoingVideoSettings(txArgs_);
}

void VideoRtpSession::updateDestination(const string &destination,
                                        unsigned int port)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (destination.empty()) {
        ERROR("Destination is empty, ignoring");
        return;
    }

    std::stringstream tmp;
    tmp << "rtp://" << destination << ":" << port;
    // if destination has changed
    if (tmp.str() != txArgs_["destination"]) {
        if (sender_) {
            ERROR("Video is already being sent");
            return;
        }
        txArgs_["destination"] = tmp.str();
        DEBUG("updated dest to %s",  txArgs_["destination"].c_str());
    }

    if (port == 0) {
        DEBUG("Sending video disabled, port was set to 0");
        sending_ = false;
    }
}

void VideoRtpSession::startSender()
{
	if (sending_) {
        if (sender_) {
            if (videoLocal_)
                videoLocal_->detach(sender_.get());
            if (videoMixer_)
                videoMixer_->detach(sender_.get());
            WARN("Restarting video sender");
        }

        try {
            sender_.reset(new VideoSender(txArgs_, *socketPair_));
        } catch (const VideoEncoderException &e) {
            ERROR("%s", e.what());
            sending_ = false;
        }
    }
}

void VideoRtpSession::startReceiver()
{
    if (receiving_) {
        if (receiveThread_)
            WARN("restarting video receiver");
        receiveThread_.reset(new VideoReceiveThread(callID_, rxArgs_));
        receiveThread_->setRequestKeyFrameCallback(&SIPVoIPLink::enqueueKeyframeRequest);
        receiveThread_->addIOContext(*socketPair_);
        receiveThread_->startLoop();
    } else {
        DEBUG("Video receiving disabled");
        if (receiveThread_)
            receiveThread_->detach(videoMixer_.get());
        receiveThread_.reset();
    }
}

void VideoRtpSession::start(int localPort)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (not sending_ and not receiving_) {
        stop();
        return;
    }

    try {
        socketPair_.reset(new SocketPair(txArgs_["destination"].c_str(), localPort));
    } catch (const std::runtime_error &e) {
        ERROR("Socket creation failed on port %d: %s", localPort, e.what());
        return;
    }

    startSender();
    startReceiver();

    // Setup video pipeline
    if (conference_)
        setupConferenceVideoPipeline(conference_);
    else if (sender_) {
        auto videoCtrl = Manager::instance().getVideoManager();
        videoLocal_ = videoCtrl->getVideoCamera();
        if (videoLocal_ and videoLocal_->attach(sender_.get()))
            videoCtrl->switchToCamera();
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
    videoMixer_->setDimensions(atol(txArgs_["width"].c_str()),
                               atol(txArgs_["height"].c_str()));

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
    if (sending_ or receiveThread_)
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

} // end namespace sfl_video
