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
#include <iostream>
#include <sstream>
#include <map>
#include <string>
#include "video_send_thread.h"
#include "video_receive_thread.h"
#include "sip/sdp.h"
#include "dbus/dbusmanager.h"
#include "dbus/callmanager.h"

namespace sfl_video {

VideoRtpSession::VideoRtpSession()
{
    txArgs_["input"]       = "/dev/video0";
    txArgs_["codec"]       = "libx264";
    txArgs_["bitrate"]     = "1000000";
    txArgs_["destination"] = "rtp://127.0.0.1:5000";
    txArgs_["format"]      = "rgb24";
    // default to CIF/SIF(625)
    txArgs_["width"]       = "352";
    txArgs_["height"]      = "288";
    txArgs_["framerate"]      = "30";

    rxArgs_["codec"] = txArgs_["codec"];
    rxArgs_["bitrate"] = txArgs_["bitrate"];
    rxArgs_["format"] = txArgs_["format"];
    rxArgs_["width"] = txArgs_["width"];
    rxArgs_["height"] = txArgs_["height"];
}

VideoRtpSession::VideoRtpSession(const std::map<std::string, std::string> &txArgs,
                const std::map<std::string, std::string> &rxArgs) :
    txArgs_(txArgs), rxArgs_(rxArgs)
{}

void VideoRtpSession::updateSDP(const Sdp *sdp)
{
    std::string desc = sdp->getActiveVideoDescription();
    // if port has changed
    if (desc != rxArgs_["receiving_sdp"])
    {
        rxArgs_["receiving_sdp"] = desc;
        _debug("%s:Updated incoming SDP to:\n %s", __PRETTY_FUNCTION__,
                rxArgs_["receiving_sdp"].c_str());
    }
}

void VideoRtpSession::updateDestination(const std::string &destination,
        unsigned int port)
{
    std::stringstream tmp;
    assert(not destination.empty());
    tmp << "rtp://" << destination << ":" << port;
    // if destination has changed
    if (tmp.str() != txArgs_["destination"])
    {
        txArgs_["destination"] = tmp.str();
        _debug("%s updated dest to %s",  __PRETTY_FUNCTION__,
               txArgs_["destination"].c_str());
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

    receiveThread_.reset(new VideoReceiveThread(rxArgs_));
    receiveThread_->start();
}

void VideoRtpSession::start()
{
    assert(sendThread_.get() == 0);
    assert(receiveThread_.get() == 0);

    sendThread_.reset(new VideoSendThread(txArgs_));
    sendThread_->start();

    receiveThread_.reset(new VideoReceiveThread(rxArgs_));
    receiveThread_->start();
}

void VideoRtpSession::stop()
{
    _debug(__PRETTY_FUNCTION__);
    if (receiveThread_.get())
        receiveThread_.reset();

    if (sendThread_.get())
        sendThread_.reset();
}
} // end namspace sfl_video
