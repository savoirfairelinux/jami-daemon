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

#pragma once

#include "socket_pair.h"
#include "sip/sip_utils.h"
#include "media/media_codec.h"

#include <string>
#include <memory>
#include <mutex>

namespace ring {

class RtpSession {
public:
    RtpSession(const std::string &callID) : callID_(callID) {}
    virtual ~RtpSession() {};

    virtual void start() = 0;
    virtual void start(std::unique_ptr<IceSocket> rtp_sock,
                       std::unique_ptr<IceSocket> rtcp_sock) = 0;
    virtual void stop() = 0;

    virtual void updateMedia(const MediaDescription& send,
                             const MediaDescription& receive) {
        send_ = send;
        receive_ = receive;
    }

    virtual void setSenderInitSeqVal(uint16_t seqVal) = 0;
    virtual uint16_t getSenderLastSeqValue() = 0;

    bool isSending() { return send_.enabled; }
    bool isReceiving() { return receive_.enabled; }

protected:
    std::recursive_mutex mutex_;
    std::unique_ptr<SocketPair> socketPair_;
    const std::string callID_;

    MediaDescription send_;
    MediaDescription receive_;

    std::string getRemoteRtpUri() const {
        return "rtp://" + send_.addr.toString(true);
    }
};

} // namespace ring
