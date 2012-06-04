/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include "video_rtp_session.h"
#include <cassert>
#include <sstream>
#include <map>
#include <string>
#include "shared_memory.h"
#include "video_send_thread.h"
#include "video_receive_thread.h"
#include "sip/sdp.h"
#include "libav_utils.h"
#include "manager.h"
#include "logger.h"

namespace sfl_video {

using std::map;
using std::string;

VideoRtpSession::VideoRtpSession(const map<string, string> &txArgs) :
    sharedMemory_(), sendThread_(), receiveThread_(), txArgs_(txArgs),
    rxArgs_(), sending_(true), receiving_(true)
{
    // FIXME: bitrate must be configurable
    txArgs_["bitrate"] = "500000";
}

VideoRtpSession::VideoRtpSession(const map<string, string> &txArgs,
                                 const map<string, string> &rxArgs) :
    sharedMemory_(), sendThread_(), receiveThread_(), txArgs_(txArgs),
    rxArgs_(rxArgs), sending_(true), receiving_(true)
{}

void VideoRtpSession::updateSDP(const Sdp &sdp)
{
    const std::vector<string> v(sdp.getActiveVideoDescription());
    const string &desc = v[0];
    // if port has changed
    if (desc != rxArgs_["receiving_sdp"]) {
        rxArgs_["receiving_sdp"] = desc;
        DEBUG("%s:Updated incoming SDP to:\n %s", __PRETTY_FUNCTION__,
              rxArgs_["receiving_sdp"].c_str());
    }

    if (desc.find("sendrecv") != string::npos) {
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

    const string codec = libav_utils::encodersMap()[v[1]];
    if (codec.empty()) {
        DEBUG("Couldn't find encoder for \"%s\"\n", v[1].c_str());
        sending_ = false;
    } else {
        txArgs_["codec"] = codec;
    }

    txArgs_["payload_type"] = v[2];
}

void VideoRtpSession::updateDestination(const string &destination,
                                        unsigned int port)
{
    assert(not destination.empty());

    std::stringstream tmp;
    tmp << "rtp://" << destination << ":" << port;
    // if destination has changed
    if (tmp.str() != txArgs_["destination"]) {
        assert(sendThread_.get() == 0);
        txArgs_["destination"] = tmp.str();
        DEBUG("%s updated dest to %s",  __PRETTY_FUNCTION__,
              txArgs_["destination"].c_str());
    }

    if (port == 0) {
        DEBUG("Sending video disabled, port was set to 0");
        sending_ = false;
    }
}

void VideoRtpSession::test()
{
    assert(sendThread_.get() == 0);
    assert(receiveThread_.get() == 0);

    sendThread_.reset(new VideoSendThread(txArgs_));
    sendThread_->start();

    /* block until SDP is ready */
    sendThread_->waitForSDP();
}

void VideoRtpSession::test_loopback()
{
    assert(sendThread_.get() == 0);

    sendThread_.reset(new VideoSendThread(txArgs_));
    sendThread_->start();

    sendThread_->waitForSDP();
    rxArgs_["input"] = "test.sdp";
    VideoControls *controls(Manager::instance().getDbusManager()->getVideoControls());
    sharedMemory_.reset(new SharedMemory(*controls));
    receiveThread_.reset(new VideoReceiveThread(rxArgs_, *sharedMemory_));
    receiveThread_->start();
}

void VideoRtpSession::start()
{
    if (sending_) {
        if (sendThread_.get())
            WARN("Restarting video sender");
        sendThread_.reset(new VideoSendThread(txArgs_));
        sendThread_->start();
    }
    else
        DEBUG("Video sending disabled");

    if (receiving_) {
        if (receiveThread_.get())
            WARN("Restarting video receiver");
        VideoControls *controls(Manager::instance().getDbusManager()->getVideoControls());
        sharedMemory_.reset(new SharedMemory(*controls));
        receiveThread_.reset(new VideoReceiveThread(rxArgs_, *sharedMemory_));
        receiveThread_->start();
        sharedMemory_->publishShm();
    }
    else
        DEBUG("Video receiving disabled");
}

void VideoRtpSession::stop()
{
    DEBUG("%s", __PRETTY_FUNCTION__);
    receiveThread_.reset();
    sendThread_.reset();
}
} // end namespace sfl_video
