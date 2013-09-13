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
#include "video_send_thread.h"
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
#include <thread>
#include <chrono>

namespace sfl_video {

using std::map;
using std::string;

VideoRtpSession::VideoRtpSession(const string &callID,
								 const map<string, string> &txArgs) :
    socketPair_(), sendThread_(), receiveThread_(), txArgs_(txArgs),
    rxArgs_(), sending_(false), receiving_(false), callID_(callID),
    videoMixer_(), videoLocal_(), sink_(new SHMSink())
{}

VideoRtpSession::~VideoRtpSession()
{ stop(); }

void VideoRtpSession::updateSDP(const Sdp &sdp)
{
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
    if (destination.empty()) {
        ERROR("Destination is empty, ignoring");
        return;
    }

    std::stringstream tmp;
    tmp << "rtp://" << destination << ":" << port;
    // if destination has changed
    if (tmp.str() != txArgs_["destination"]) {
        if (sendThread_.get() != 0) {
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
	std::string curcid = Manager::instance().getCurrentCallId();

    videoMixer_ = nullptr;
    videoLocal_ = nullptr;

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
        if (!videoCtrl->hasPreviewStarted()) {
            videoCtrl->startPreview();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Check for video conference mode
        auto conf = Manager::instance().getConferenceFromCallID(callID_);
        videoLocal_ = videoCtrl->getVideoPreview();
        if (not conf and not videoLocal_) {
            ERROR("Sending disabled, no local video");
            sending_ = false;
            sendThread_.reset();
        } else {
            if (conf) {
                // setup mixer pipeline
                videoMixer_ = conf->getVideoMixer();
                if (videoLocal_)
                    videoLocal_->attach(videoMixer_);
            }

            if (sendThread_.get())
                WARN("Restarting video sender");

            sendThread_.reset(new VideoSendThread(callID_, txArgs_,
                                                  *socketPair_, videoLocal_,
                                                  videoMixer_));
        }
    } else {
        DEBUG("Video sending disabled");
        sendThread_.reset();
    }

    if (receiving_) {
        if (receiveThread_.get())
            WARN("restarting video receiver");
        receiveThread_.reset(new VideoReceiveThread(callID_, rxArgs_,
                                                    !videoMixer_?sink_:nullptr));
        receiveThread_->setRequestKeyFrameCallback(&SIPVoIPLink::enqueueKeyframeRequest);
        receiveThread_->addIOContext(*socketPair_);
        receiveThread_->start();
    } else {
        DEBUG("Video receiving disabled");
        receiveThread_.reset();
    }
}

void VideoRtpSession::stop()
{
    Manager::instance().getVideoControls()->stoppedDecoding(callID_,
                                                            sink_->openedName());
    if (videoLocal_ and videoMixer_) {
        videoLocal_->detach(videoMixer_);
        videoMixer_->detach(sink_.get());
    } else if (videoLocal_)
        videoLocal_->detach(sink_.get());
    else if (videoMixer_)
        videoMixer_->detach(sink_.get());

    videoLocal_ = nullptr;
    videoMixer_ = nullptr;

    if (socketPair_.get())
        socketPair_->interrupt();

    receiveThread_.reset();
    sendThread_.reset();
    socketPair_.reset();
}

void VideoRtpSession::forceKeyFrame()
{
    if (sendThread_.get())
        sendThread_->forceKeyFrame();
}

void VideoRtpSession::addReceivingDetails(std::map<std::string, std::string> &details)
{
    if (receiveThread_.get()) {
        details["VIDEO_SHM_PATH"] = sink_->openedName();
        std::ostringstream os;
        os << receiveThread_->getWidth();
        details["VIDEO_WIDTH"] = os.str();
        os.str("");
        os << receiveThread_->getHeight();
        details["VIDEO_HEIGHT"] = os.str();
    }
}

} // end namespace sfl_video
