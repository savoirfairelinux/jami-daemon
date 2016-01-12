/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
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

#include "media_io_handle.h"
#include "threadloop.h"

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
#include <queue>
#include <vector>
#include <condition_variable>
#include <bitset>
#include <algorithm>
#include <chrono>

//flag to display RTCP XR packet sent
#define DUMP_STAT
namespace ring {

class IceSocket;
class SRTPProtoContext;
class RtpStats; // Private

#pragma pack(4) // Pack to 32bits all following structures

// RFC 3550 - Common part of all RTP/RTCP packet - (one uint32_t)
union rtpBase {
    struct {
#ifdef WORDS_BIGENDIAN
        uint32_t version:2;     // protocol version
        uint32_t p:1;           // padding flag
        uint32_t count:5;       // mean depends on PT
        uint32_t m_pt:8;          // payload type
        uint32_t seq_len:16;    // sequence # of RTP packet or length of RTCP packet
#else
        uint32_t seq_len:16;
        uint32_t m_pt:8;
        uint32_t count:5;
        uint32_t p:1;
        uint32_t version:2;
#endif
    } s;
    uint32_t n;
};

// RFC 3550
struct rtpHead {
    rtpBase base;
    uint32_t ts;                // timestamp
    uint32_t ssrc;              // synchronization source

    // CSRC list follow (zero or more)
};


// RFC 3550
struct rtcpReportHead {
    rtpBase base;               // common RTP/RTCP header
    uint32_t ssrc;              // synchronization source identifier of packet send

    // Block reports follow (zero or more). See rtcpRRBlock and rtcpXRBlock
};

// RFC 3550
struct rtcpRRBlock {
    uint32_t ssrc_n;            // synchronization source identifier of source #n
    union {
        struct {
#ifdef WORDS_BIGENDIAN
            uint32_t fraction_lost:8;   // fraction lost
            uint32_t cum_pkt_lost:24;   // cumulative packets lost
#else
            uint32_t cum_pkt_lost:24;
            uint32_t fraction_lost:8;
#endif
        } s;
        uint32_t n;
    } lost_stats;
    uint32_t last_seq;          // last sequence number
    uint32_t jitter;            // jitter
};

// RFC 3611
union rtcpXRBlockHead {
    struct {
#ifdef WORDS_BIGENDIAN
        uint32_t bt:4;          // block type = 1 in our case
        uint32_t reserved:4;    // reserved
        uint32_t thinning:4;    // thinning ?
        uint32_t len:16;        // block length
#else
        uint32_t len:16;
        uint32_t thinning:4;
        uint32_t reserved:4;
        uint32_t bt:4;
#endif
    } s;
    uint32_t n;
};

// RFC 3611
struct rtcpXRBlockLoss {
    rtcpXRBlockHead head;
    uint32_t ssrc;              // synchronization source identifier of packet send
    uint16_t begin_seq;         // The first sequence number that this block reports on
    uint16_t end_seq;           // The last sequence number that this block reports on

    // XR block chunk follow (one or more, count = end_seq - begin_seq + 1)
};

using rtcpXRBlockContent = std::vector<uint8_t>;

#pragma pack(0)

class SocketPair {
    public:
        SocketPair(const char* uri, int localPort);
        SocketPair(std::unique_ptr<IceSocket> rtp_sock,
                   std::unique_ptr<IceSocket> rtcp_sock);
        ~SocketPair();

        void interrupt();

        MediaIOHandle* createIOContext();

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
        std::vector<std::vector<uint8_t>> getRawRtcpPackets();

    private:
        NON_COPYABLE(SocketPair);

        struct RawPacket: std::vector<uint8_t> {
            RawPacket(const uint8_t* buf, std::size_t len);
        };

        int readCallback(uint8_t* buf, int buf_size);
        int writeCallback(uint8_t* buf, int buf_size);

        int waitForData();
        int readRtpData(void* buf, int buf_size);
        int readRtcpData(void* buf, int buf_size);
        int writeData(uint8_t* buf, int buf_size);

        std::mutex dataBuffMutex_;
        std::condition_variable cv_;
        std::list<std::vector<uint8_t>> rtpDataBuff_;
        std::list<std::vector<uint8_t>> rtcpDataBuff_;

        std::unique_ptr<IceSocket> rtp_sock_;
        std::unique_ptr<IceSocket> rtcp_sock_;

        int rtpHandle_ {-1};
        int rtcpHandle_ {-1};
        sockaddr_storage rtpDestAddr_;
        socklen_t rtpDestAddrLen_;
        sockaddr_storage rtcpDestAddr_;
        socklen_t rtcpDestAddrLen_;
        std::atomic_bool interrupted_ {false};
        std::atomic_bool noWrite_ {false};
        std::unique_ptr<SRTPProtoContext> srtpContext_;

        void saveRawRtcpPacket(const uint8_t* buf, std::size_t len);
        std::mutex rtcpPacketMutex_;
        std::queue<RawPacket, std::list<RawPacket>> rtcpPacketQueue_;
        static constexpr unsigned MAX_RTCP_PACKETS {20};

    void analyseRawRTPPacket(const uint8_t* buf, std::size_t len);
    void dumpRtpStats(const RtpStats& s);

    std::unique_ptr<RtpStats> rtpStats_;


    std::chrono::time_point<std::chrono::steady_clock> lastRtpStatDump_;
    std::chrono::time_point<std::chrono::steady_clock> lastRtpRxTime_;

#if 0
        rtcpXRPacket rtcpXRPacket_;
#ifdef DUMP_STAT
        void dumpRTPStats();
#endif
        void addSeqToBlkContent(bool isLost);
        static constexpr unsigned XR_BLOCK_CONTENT_MAX {14};
        uint16_t lastSeq_ {0};
        unsigned packetDropped_ {0};
        unsigned packetReceived_ {0};
        unsigned packetDuplicated_ {0};
        std::bitset<16> blkContent_ {std::bitset<16>{}.set(0)};
        unsigned blkContentIndex_ {0};
        bool newRtcpReport_ {true};
        std::vector<uint16_t> missingCseq_;
    //std::mutex rtpCheckingMutex_;
        std::chrono::time_point<std::chrono::steady_clock> lastRTPCheck_;
        static constexpr unsigned RTP_STAT_INTERVAL {4};
        static constexpr bool PACKET_LOST = true;
        void sendRTCPXR();
        void addContentToSerializedQueue(uint16_t content);
#endif

    friend struct RtpStats;
};

} // namespace ring
