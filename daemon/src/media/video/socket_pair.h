/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Copyright (C) 2012 VLC authors and VideoLAN
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

#ifndef SOCKET_PAIR_H_
#define SOCKET_PAIR_H_

#include "video_base.h"

#include <sys/socket.h>
#include <mutex>
#include <stdint.h>

namespace ring {
class IceSocket;
};

namespace ring { namespace video {

class SocketPair {
    public:
        SocketPair(const char *uri, int localPort);
        SocketPair(std::unique_ptr<ring::IceSocket> rtp_sock,
                   std::unique_ptr<ring::IceSocket> rtcp_sock);
        ~SocketPair();

        void interrupt();
        VideoIOHandle* createIOContext();
        void openSockets(const char *uri, int localPort);
        void closeSockets();

    private:
        NON_COPYABLE(SocketPair);

        static int readCallback(void *opaque, uint8_t *buf, int buf_size);
        static int writeCallback(void *opaque, uint8_t *buf, int buf_size);

        int waitForData();
        int readRtpData(void *buf, int buf_size);
        int readRtcpData(void *buf, int buf_size);
        int writeRtpData(void *buf, int buf_size);
        int writeRtcpData(void *buf, int buf_size);

        std::unique_ptr<ring::IceSocket> rtp_sock_;
        std::unique_ptr<ring::IceSocket> rtcp_sock_;

        std::mutex rtcpWriteMutex_;

        int rtpHandle_ {-1};
        int rtcpHandle_ {-1};
        sockaddr_storage rtpDestAddr_;
        socklen_t rtpDestAddrLen_;
        sockaddr_storage rtcpDestAddr_;
        socklen_t rtcpDestAddrLen_;
        bool interrupted_ {false};
};

}}

#endif  // SOCKET_PAIR_H_
