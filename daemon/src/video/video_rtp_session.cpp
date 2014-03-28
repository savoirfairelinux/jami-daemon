/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "client/video_controls.h"
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
    mutex_(), socketPair_(), sender_(), receiveThread_(), txArgs_(txArgs),
    rxArgs_(), sending_(false), receiving_(false), callID_(callID),
    videoMixerSP_(), videoLocal_()
{}

VideoRtpSession::~VideoRtpSession()
{ stop(); }

void VideoRtpSession::updateSDP(const Sdp &sdp)
{
    std::lock_guard<std::mutex> lock(mutex_);

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
    std::lock_guard<std::mutex> lock(mutex_);
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

void VideoRtpSession::start(int localPort)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (not sending_ and not receiving_)
        return;

    try {
        socketPair_.reset(new SocketPair(txArgs_["destination"].c_str(), localPort));
    } catch (const std::runtime_error &e) {
        ERROR("Socket creation failed on port %d: %s", localPort, e.what());
        return;
    }

	if (sending_) {
        // Local video startup if needed
        auto videoCtrl = Manager::instance().getVideoControls();
        const bool firstStart = not videoCtrl->hasCameraStarted();
        videoCtrl->startCamera();
        if (firstStart)
            MYSLEEP(1);

        videoLocal_ = videoCtrl->getVideoCamera();
        if (sender_)
            WARN("Restarting video sender");

        try {
            sender_.reset(new VideoSender(callID_, txArgs_, *socketPair_));
        } catch (const VideoSenderException &e) {
            ERROR("%s", e.what());
            sending_ = false;
        }
    }

    /* sending may have been set to false in previous block */
    if (not sending_) {
        DEBUG("Video sending disabled");
        if (auto shared = videoLocal_.lock())
            shared->detach(sender_.get());
        if (videoMixerSP_)
            videoMixerSP_->detach(sender_.get());
        sender_.reset();
    }

    if (receiving_) {
        if (receiveThread_)
            WARN("restarting video receiver");
        receiveThread_.reset(new VideoReceiveThread(callID_, rxArgs_));
        receiveThread_->setRequestKeyFrameCallback(&SIPVoIPLink::enqueueKeyframeRequest);
        receiveThread_->addIOContext(*socketPair_);
        receiveThread_->start();
    } else {
        DEBUG("Video receiving disabled");
        if (receiveThread_)
            receiveThread_->detach(videoMixerSP_.get());
        receiveThread_.reset();
    }

    // Setup pipeline
    if (videoMixerSP_) {
        setupConferenceVideoPipeline();
    } else if (auto shared = videoLocal_.lock()) {
        if (sender_)
            shared->attach(sender_.get());
    }
}

void VideoRtpSession::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto shared = videoLocal_.lock())
        shared->detach(sender_.get());

    if (videoMixerSP_) {
        videoMixerSP_->detach(sender_.get());
        if (receiveThread_)
            receiveThread_->detach(videoMixerSP_.get());
    }

    if (socketPair_)
        socketPair_->interrupt();

    receiveThread_.reset();
    sender_.reset();
    socketPair_.reset();
    auto videoCtrl = Manager::instance().getVideoControls();
    videoCtrl->stopCamera();
}

void VideoRtpSession::forceKeyFrame()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (sender_)
        sender_->forceKeyFrame();
}

void VideoRtpSession::addReceivingDetails(std::map<std::string, std::string> &details)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (receiveThread_)
        receiveThread_->addReceivingDetails(details);
}

void VideoRtpSession::setupConferenceVideoPipeline()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sender_)
        return;

    videoMixerSP_->setDimensions(atol(txArgs_["width"].c_str()),
                                 atol(txArgs_["height"].c_str()));
    if (auto shared = videoLocal_.lock())
        shared->detach(sender_.get());
    videoMixerSP_->attach(sender_.get());

    if (receiveThread_) {
        receiveThread_->enterConference();
        receiveThread_->attach(videoMixerSP_.get());
    }
}

void VideoRtpSession::enterConference(Conference *conf)
{
    std::lock_guard<std::mutex> lock(mutex_);
    /* Detach from a possible previous conference */
    exitConference();
    videoMixerSP_ = std::move(conf->getVideoMixer());

    setupConferenceVideoPipeline();
}

void VideoRtpSession::exitConference()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (videoMixerSP_) {
        if (sender_)
            videoMixerSP_->detach(sender_.get());

        if (receiveThread_) {
            receiveThread_->detach(videoMixerSP_.get());
            receiveThread_->exitConference();
        }

        videoMixerSP_.reset();
    }

    if (auto shared = videoLocal_.lock())
        shared->attach(sender_.get());
}

} // end namespace sfl_video
