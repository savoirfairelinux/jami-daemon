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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "media_io_handle.h"

#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_t = int;
#endif

#include <mutex>
#include <memory>
#include <atomic>
#include <list>
#include <vector>

#include <cstdint>
#include <condition_variable>

namespace ring {

class IceSocket;
class SRTPProtoContext;

struct RtcpInfo {
    uint8_t fraction_lost = 0;
    uint32_t ssrc = 0;
    uint32_t ssrc_1 = 0;
    uint32_t jitter = 0;
};

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
        std::shared_ptr<RtcpInfo> getRtcpInfo();

    private:
        NON_COPYABLE(SocketPair);

        static int readCallback(void *opaque, uint8_t *buf, int buf_size);
        static int writeCallback(void *opaque, uint8_t *buf, int buf_size);

        int waitForData();
        int readRtpData(void *buf, int buf_size);
        int readRtcpData(void *buf, int buf_size);
        int writeRtpData(void *buf, int buf_size);
        int writeRtcpData(void *buf, int buf_size);
        void parseRtcpPacket(uint8_t* buf, size_t len);

        std::mutex dataReceivedMutex_;
        std::condition_variable cv_;
        std::list<std::vector<uint8_t>> dataBuff_;
        std::atomic<bool> canRead_ {false};

        std::unique_ptr<IceSocket> rtp_sock_;
        std::unique_ptr<IceSocket> rtcp_sock_;

        std::mutex rtcpWriteMutex_;

        int rtpHandle_ {-1};
        int rtcpHandle_ {-1};
        sockaddr_storage rtpDestAddr_;
        socklen_t rtpDestAddrLen_;
        sockaddr_storage rtcpDestAddr_;
        socklen_t rtcpDestAddrLen_;
        std::atomic_bool interrupted_ {false};
        std::unique_ptr<SRTPProtoContext> srtpContext_;

        std::shared_ptr<RtcpInfo> rtcpInfo_;

};


typedef struct {
#ifdef WORDS_BIGENDIAN
    uint32_t version:2; /* protocol version */
    uint32_t p:1;       /* padding flag */
    uint32_t rc:5;      /* reception report count must be 201 for report */

#else
    uint32_t rc:5;      /* reception report count must be 201 for report */
    uint32_t p:1;       /* padding flag */
    uint32_t version:2; /* protocol version */
#endif
    uint32_t pt:8;      /* payload type */
    uint32_t len:16;    /* length of RTCP packet */
    uint32_t ssrc;      /* synchronization source identifier of packet send */
    uint32_t ssrc_1;    /* synchronization source identifier of first source */
    uint32_t fraction_lost; /* 8 bits of fraction, 24 bits of total packets lost */
    uint32_t last_seq;  /*last sequence number */
    uint32_t jitter;    /*jitter */
} rtcpRRHeader;


} // namespace ring

#endif  // SOCKET_PAIR_H_
