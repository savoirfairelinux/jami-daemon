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
#include <sstream>
#include <map>
#include <string>
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
    sendThread_(), receiveThread_(), txArgs_(txArgs),
    rxArgs_(), sending_(false), receiving_(false)
{
    // FIXME: bitrate must be configurable
    txArgs_["bitrate"] = "500000";
}

void VideoRtpSession::updateSDP(const Sdp &sdp)
{
    const std::vector<string> v(sdp.getActiveVideoDescription());
    const string &desc = v[0];
    // if port has changed
    if (desc != rxArgs_["receiving_sdp"]) {
        rxArgs_["receiving_sdp"] = desc;
        DEBUG("Updated incoming SDP to:\n %s",
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

    if (not v[1].empty()) {
        const string codec = libav_utils::encodersMap()[v[1]];
        if (codec.empty()) {
            DEBUG("Couldn't find encoder for \"%s\"\n", v[1].c_str());
            sending_ = false;
        } else {
            txArgs_["codec"] = codec;
        }
    } else {
        sending_ = false;
    }

    txArgs_["payload_type"] = v[2];
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
        receiveThread_.reset(new VideoReceiveThread(rxArgs_));
        receiveThread_->start();
    }
    else
        DEBUG("Video receiving disabled");
}

void VideoRtpSession::stop()
{
    receiveThread_.reset();
    sendThread_.reset();
}
} // end namespace sfl_video
