/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *  Copyright (C) 2012 VLC authors and VideoLAN
 *
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
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ip_utils.h"
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

#include <cstdint>
#include <mutex>
#include <memory>
#include <atomic>
#include <list>
#include <vector>
#include <condition_variable>
#include <functional>

namespace jami {

class IceSocket;
class SRTPProtoContext;

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
    uint32_t id;        /* synchronization source identifier of first source */
    uint32_t fraction_lost:8;       /* 8 bits of fraction, 24 bits of total packets lost */
    uint32_t cum_lost_packet:24;    /* cumulative number packet lost */
    uint32_t ext_high;  /* Extended highest sequence number received */
    uint32_t jitter;    /* jitter */
    uint32_t lsr;       /* last SR timestamp */
    uint32_t dlsr;      /* Delay since last SR timestamp */
} rtcpRRHeader;

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
    uint32_t timestampMSB;      /* timestamp MSB */
    uint32_t timestampLSB;      /* timestamp LSB */
    uint32_t timestampRTP;      /* RTP timestamp */
    uint32_t spc;       /* Sender's packet count */
    uint32_t soc;       /* Sender's octet count */
} rtcpSRHeader;


class SocketPair {
    public:
        SocketPair(const char* uri, int localPort);
        SocketPair(std::unique_ptr<IceSocket> rtp_sock,
                   std::unique_ptr<IceSocket> rtcp_sock);
        ~SocketPair();

        void interrupt();

        MediaIOHandle* createIOContext(const uint16_t mtu);

        void openSockets(const char* uri, int localPort);
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
        void createSRTP(const char* out_suite, const char* out_params,
                        const char* in_suite, const char* in_params);

        void stopSendOp(bool state = true);
        std::list<rtcpRRHeader> getRtcpInfo();

        bool waitForRTCP(std::chrono::seconds interval);
        double getLastLatency();

        void setPacketLossCallback(std::function<void (void)> cb);
        void setRtpDelayCallback(std::function<void (int)> cb);

    private:
        NON_COPYABLE(SocketPair);

        int readCallback(uint8_t* buf, int buf_size);
        int writeCallback(uint8_t* buf, int buf_size);

        int waitForData();
        int readRtpData(void* buf, int buf_size);
        int readRtcpData(void* buf, int buf_size);
        int writeData(uint8_t* buf, int buf_size);
        void saveRtcpPacket(uint8_t* buf, size_t len);

        std::mutex dataBuffMutex_;
        std::condition_variable cv_;
        std::list<std::vector<uint8_t>> rtpDataBuff_;
        std::list<std::vector<uint8_t>> rtcpDataBuff_;

        std::unique_ptr<IceSocket> rtp_sock_;
        std::unique_ptr<IceSocket> rtcp_sock_;

        int rtpHandle_ {-1};
        int rtcpHandle_ {-1};
        IpAddr rtpDestAddr_;
        IpAddr rtcpDestAddr_;
        std::atomic_bool interrupted_ {false};
        std::atomic_bool noWrite_ {false};
        std::unique_ptr<SRTPProtoContext> srtpContext_;
        std::function<void(void)> packetLossCallback_;
        std::function<void(int)> rtpDelayCallback_;
        int32_t getOneWayDelayGradient(uint32_t sendTS);

        std::list<rtcpRRHeader> listRtcpHeader_;
        std::mutex rtcpInfo_mutex_;
        std::condition_variable cvRtcpPacketReadyToRead_;
        static constexpr unsigned MAX_LIST_SIZE {10};

        mutable std::atomic_bool rtcpPacketLoss_ {false};
        double lastSRTS_;
        uint32_t lastDLSR_;

        std::list<double> histoLatency_;

        std::chrono::steady_clock::time_point lastRR_time;
        uint16_t lastSeqNum_ {0};
        uint32_t lastSendTS_ {0};
        std::chrono::steady_clock::time_point lastReceiveTS_ {};
};


} // namespace jami
