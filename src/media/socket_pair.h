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

#include "media_io_handle.h"

#include <sys/socket.h>
#include <mutex>
#include <stdint.h>
#include <memory>

namespace ring {

class IceSocket;
class SRTPProtoContext;

class SocketPair {
    public:
        SocketPair(const char *uri, int localPort);
        SocketPair(std::unique_ptr<IceSocket> rtp_sock,
                   std::unique_ptr<IceSocket> rtcp_sock);
        ~SocketPair();

        void interrupt();

        MediaIOHandle* createIOContext();

        void openSockets(const char *uri, int localPort);
        void closeSockets();

        /*
           Supported suites are:

           AES_CM_128_HMAC_SHA1_80
           SRTP_AES128_CM_HMAC_SHA1_80
           AES_CM_128_HMAC_SHA1_32
           SRTP_AES128_CM_HMAC_SHA1_3

           Example (unsecure) usage:
           createSRTP("AES_CM_128_HMAC_SHA1_80",
                      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn",
                      "AES_CM_128_HMAC_SHA1_80",
                      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn");

           Will throw an std::runtime_error on failure, should be handled at a higher level
        */
        void createSRTP(const char *out_suite, const char *out_params, const char *in_suite, const char *in_params);

    private:
        NON_COPYABLE(SocketPair);

        static int readCallback(void *opaque, uint8_t *buf, int buf_size);
        static int writeCallback(void *opaque, uint8_t *buf, int buf_size);

        int waitForData();
        int readRtpData(void *buf, int buf_size);
        int readRtcpData(void *buf, int buf_size);
        int writeRtpData(void *buf, int buf_size);
        int writeRtcpData(void *buf, int buf_size);

        std::unique_ptr<IceSocket> rtp_sock_;
        std::unique_ptr<IceSocket> rtcp_sock_;

        std::mutex rtcpWriteMutex_;

        int rtpHandle_ {-1};
        int rtcpHandle_ {-1};
        sockaddr_storage rtpDestAddr_;
        socklen_t rtpDestAddrLen_;
        sockaddr_storage rtcpDestAddr_;
        socklen_t rtcpDestAddrLen_;
        bool interrupted_ {false};
        std::unique_ptr<SRTPProtoContext> srtpContext_;
};

} // namespace ring

#endif  // SOCKET_PAIR_H_
