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
#include <vector>
#include <condition_variable>
#include <bitset>

//flag to display RTCP XR packet sent
#define DUMP_STAT
namespace ring {

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
    uint32_t ssrc_1;    /* synchronization source identifier of first source */
    uint32_t fraction_lost; /* 8 bits of fraction, 24 bits of total packets lost */
    uint32_t last_seq;  /*last sequence number */
    uint32_t jitter;    /*jitter */
} rtcpRRHeader;

typedef struct {
#ifdef WORDS_BIGENDIAN
    uint32_t version:2; /* protocol version */
    uint32_t p:1;       /* padding flag */
    uint32_t reserved:5;/* field reserved*/

#else
    uint32_t reserved:5;/* field reserved*/
    uint32_t p:1;       /* padding flag */
    uint32_t version:2; /* protocol version */
#endif
    uint8_t pt;      /* payload type */
    uint16_t len;    /* length of RTCP packet */
    uint32_t ssrc;      /* synchronization source identifier of packet send */
} rtcpXRHeader;

typedef struct {
    uint8_t bt;      /* block type = 1 in our case */
#ifdef WORDS_BIGENDIAN
    uint32_t reserved:4; /* reserved */
    uint32_t thinning:4; /* thinning ? */

#else
    uint32_t thinning:4; /* thinning ? */
    uint32_t reserved:4; /* reserved */
#endif
    uint16_t len;    /* block length */
    uint32_t ssrc;      /* synchronization source identifier of packet send */
    uint16_t beginSeq;/* The first sequence number that this block reports on */
    uint16_t endSeq;  /* The last sequence number that this block reports on */
} rtcpXRBlockHeader;

typedef std::vector<std::bitset<16>> rtcpXRBlockContent;
//typedef std::vector<uint16_t> rtcpXRBlockContent;

typedef struct {
    rtcpXRHeader xrHeader;
    rtcpXRBlockHeader blkHeader;
    rtcpXRBlockContent blkContent;
} rtcpXRPacket;

typedef struct {
#ifdef WORDS_BIGENDIAN
    uint32_t version:2;   /* protocol version */
    uint32_t p:1;           /* padding flag */
    uint32_t x:1;           /* header extension flag */
    uint32_t cc:4;          /* CSRC count */
    uint32_t m:1;           /* marker bit */
    uint32_t pt:7;          /* payload type */
#else
    uint32_t cc:4;          /* CSRC count */
    uint32_t x:1;           /* header extension flag */
    uint32_t p:1;           /* padding flag */
    uint32_t version:2;         /* protocol version */
    uint32_t pt:7;          /* payload type */
    uint32_t m:1;           /* marker bit */
#endif
    uint32_t seq:16;        /* sequence number */
    uint32_t ts;                /* timestamp */
    uint32_t ssrc;              /* synchronization source */
    uint32_t csrc[1];           /* optional CSRC list */
} rtpHeader;

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
        std::vector<rtcpRRHeader> getRtcpInfo();

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
        sockaddr_storage rtpDestAddr_;
        socklen_t rtpDestAddrLen_;
        sockaddr_storage rtcpDestAddr_;
        socklen_t rtcpDestAddrLen_;
        std::atomic_bool interrupted_ {false};
        std::atomic_bool noWrite_ {false};
        std::unique_ptr<SRTPProtoContext> srtpContext_;

        std::list<rtcpRRHeader> listRtcpHeader_;
        std::mutex rtcpInfo_mutex_;
        static constexpr unsigned MAX_LIST_SIZE {20};

        rtcpXRPacket rtcpXRPacket_;

        void checkRTPPacket(rtpHeader* header);
#ifdef DUMP_STAT
        void dumpRTPStats();
#endif
        void addSeqToBlkContent(bool isLost);
        static constexpr unsigned XR_BLOCK_CONTENT_MAX {14};
        uint16_t lastSeq_ = 0;
        unsigned packetDropped_ = 0;
        unsigned packetReceived_ = 0;
        unsigned packetDuplicated_ = 0;
        std::bitset<16> blkContent_ = std::bitset<16>{}.set(0);
        unsigned blkContentIndex_ = 0;
        bool newRtcpReport_ = true;
        std::vector<uint16_t> missingCseq_;
        std::mutex rtpCheckingMutex_;
        std::chrono::time_point<std::chrono::system_clock>  lastRTPCheck_;
        static constexpr unsigned RTP_STAT_INTERVAL {4};
        static constexpr bool PACKET_LOST = true;
        void initRTCPXR();
        void sendRTCPXR();
};
} // namespace ring
