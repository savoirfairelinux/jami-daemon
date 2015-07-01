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

#include <cstdint>
#include <map>

#define DEBUG_RTP_DATA 1

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
        std::atomic_bool interrupted_ {false};
        std::unique_ptr<SRTPProtoContext> srtpContext_;


#if DEBUG_RTP_DATA

#ifdef WORDS_BIGENDIAN
#define RTP_BIG_ENDIAN 1
#else
#define RTP_LITTLE_ENDIAN 1
#endif
        enum RTP_DIRECTION: unsigned {
            RTP_SEND,
            RTP_RECEIVE
        };

        typedef struct {
#if RTP_BIG_ENDIAN
            //unsigned int seq:16;      /* sequence number */
            const unsigned version:2;   /* protocol version */
            unsigned int p:1;           /* padding flag */
            unsigned int x:1;           /* header extension flag */
            unsigned int cc:4;          /* CSRC count */
            unsigned int m:1;           /* marker bit */
            unsigned int pt:7;          /* payload type */
#elif RTP_LITTLE_ENDIAN
            unsigned int cc:4;          /* CSRC count */
            unsigned int x:1;           /* header extension flag */
            unsigned int p:1;           /* padding flag */
            unsigned version:2;         /* protocol version */
            unsigned int pt:7;          /* payload type */
            unsigned int m:1;           /* marker bit */
            //unsigned int seq:16;      /* sequence number */
#else
#error Define one of RTP_LITTLE_ENDIAN or RTP_BIG_ENDIAN
#endif
            unsigned int seq:16;        /* sequence number */
            uint32_t ts;                /* timestamp */
            uint32_t ssrc;              /* synchronization source */
            uint32_t csrc[1];           /* optional CSRC list */
        } rtpHeader;

        std::multimap<std::string,unsigned> mapRtpSend_;
        std::multimap<std::string,unsigned> mapRtpReceive_;

        const std::string rtp_version_   = "VERSION";
        const std::string rtp_pt_        = "PT";
        const std::string rtp_marker_    = "MARKER";
        const std::string rtp_seq_       = "SEQ";
        const std::string rtp_ts_        = "TS";

        bool mustRestartChronoSend_ = false;
        bool mustRestartChronoReceive_ = false;

        std::chrono::high_resolution_clock::time_point firstPacketSendTime_;
        std::chrono::high_resolution_clock::time_point currentPacketSendTime_;

        std::chrono::high_resolution_clock::time_point firstPacketReceiveTime_;
        std::chrono::high_resolution_clock::time_point currentPacketReceiveTime_;

        uint32_t current_ssrcSend_ = 0;
        uint16_t firstSessionSeqSend_ = 0;
        uint16_t lastSessionSeqSend_ = 0;

        uint32_t current_ssrcReceive_ = 0;
        uint16_t firstSessionSeqReceive_ = 0;
        uint16_t lastSessionSeqReceive_ = 0;

        bool addRtpHeadersInfo(void* buf, int buf_size, RTP_DIRECTION direction);
        void dumpRtpMap(unsigned period, RTP_DIRECTION direction);
        std::mutex rtpHeaderMutex_;

#define REFRESH_DUMP_PERIOD 10

#endif // DEBUG_RTP_DATA

};



} // namespace ring

#endif  // SOCKET_PAIR_H_
