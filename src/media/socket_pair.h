/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *  Copyright (C) 2012 VLC authors and VideoLAN
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "media_io_handle.h"
#include "rtp_pacer.h"
#include "transport_cc.h"

#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_t = int;
#endif

#include <dhtnet/ip_utils.h>
#include <dhtnet/ice_socket.h>

#include <cstdint>
#include <mutex>
#include <memory>
#include <atomic>
#include <deque>
#include <list>
#include <vector>
#include <condition_variable>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace dht {
namespace crypto {
struct Certificate;
struct PrivateKey;
} // namespace crypto
} // namespace dht

namespace jami {

class SRTPProtoContext;

typedef struct
{
#ifdef WORDS_BIGENDIAN
    uint32_t version : 2; /* protocol version */
    uint32_t p : 1;       /* padding flag */
    uint32_t rc : 5;      /* reception report count must be 201 for report */

#else
    uint32_t rc : 5;      /* reception report count must be 201 for report */
    uint32_t p : 1;       /* padding flag */
    uint32_t version : 2; /* protocol version */
#endif
    uint32_t pt : 8;               /* payload type */
    uint32_t len : 16;             /* length of RTCP packet */
    uint32_t ssrc;                 /* synchronization source identifier of packet send */
    uint32_t id;                   /* synchronization source identifier of first source */
    uint32_t fraction_lost : 8;    /* 8 bits of fraction, 24 bits of total packets lost */
    uint32_t cum_lost_packet : 24; /* cumulative number packet lost */
    uint32_t ext_high;             /* Extended highest sequence number received */
    uint32_t jitter;               /* jitter */
    uint32_t lsr;                  /* last SR timestamp */
    uint32_t dlsr;                 /* Delay since last SR timestamp */
} rtcpRRHeader;

typedef struct
{
#ifdef WORDS_BIGENDIAN
    uint32_t version : 2; /* protocol version */
    uint32_t p : 1;       /* padding flag */
    uint32_t rc : 5;      /* reception report count must be 201 for report */

#else
    uint32_t rc : 5;      /* reception report count must be 201 for report */
    uint32_t p : 1;       /* padding flag */
    uint32_t version : 2; /* protocol version */
#endif
    uint32_t pt : 8;       /* payload type */
    uint32_t len : 16;     /* length of RTCP packet */
    uint32_t ssrc;         /* synchronization source identifier of packet send */
    uint32_t timestampMSB; /* timestamp MSB */
    uint32_t timestampLSB; /* timestamp LSB */
    uint32_t timestampRTP; /* RTP timestamp */
    uint32_t spc;          /* Sender's packet count */
    uint32_t soc;          /* Sender's octet count */
} rtcpSRHeader;

typedef struct
{
#ifdef WORDS_BIGENDIAN
    uint32_t version : 2; /* protocol version */
    uint32_t p : 1;       /* padding flag always 0 */
    uint32_t fmt : 5;     /* Feedback message type always 15 */

#else
    uint32_t fmt : 5;     /* Feedback message type always 15 */
    uint32_t p : 1;       /* padding flag always 0 */
    uint32_t version : 2; /* protocol version */
#endif
    uint32_t pt : 8;         /* payload type */
    uint32_t len : 16;       /* length of RTCP packet */
    uint32_t ssrc;           /* synchronization source identifier of packet sender */
    uint32_t ssrc_source;    /* synchronization source identifier of first source alway 0*/
    uint32_t uid;            /* Unique identifier Always ‘R’ ‘E’ ‘M’ ‘B’ (4 ASCII characters). */
    uint32_t n_ssrc : 8;     /* Number of SSRCs in this message. */
    uint32_t br_exp : 6;     /* BR Exp */
    uint32_t br_mantis : 18; /* BR Mantissa */
    uint32_t f_ssrc;         /* SSRC feedback */
} rtcpREMBHeader;

typedef struct
{
    uint64_t last_send_ts;
    std::chrono::steady_clock::time_point last_receive_ts;
    uint64_t send_ts;
    std::chrono::steady_clock::time_point receive_ts;
} TS_Frame;

struct DtlsSrtpContext;
enum class DtlsSetup : uint8_t;

class SocketPair
{
public:
    struct BundleContext;

    static std::shared_ptr<BundleContext> createBundleContext(std::unique_ptr<dhtnet::IceSocket> rtp_sock,
                                                              std::unique_ptr<dhtnet::IceSocket> rtcp_sock,
                                                              bool rtcpMux = false,
                                                              bool installReceiveCallbacks = true);

    // Runs (once) the DTLS-SRTP handshake on the shared bundle transport
    // (RFC 8843 + RFC 5764: a single DTLS association per transport) and then
    // installs the bundle receive callbacks. Concurrent callers block until
    // the first handshake completes and share its result. Throws on failure
    // or when aborted.
    static DtlsSrtpContext ensureBundleDtlsContext(const std::shared_ptr<BundleContext>& bundleContext,
                                                   DtlsSetup localSetup,
                                                   std::string_view remoteFingerprintType,
                                                   std::string_view remoteFingerprint,
                                                   const std::shared_ptr<dht::crypto::Certificate>& certificate,
                                                   const std::shared_ptr<dht::crypto::PrivateKey>& privateKey,
                                                   const std::shared_ptr<std::atomic_bool>& abort);
    static void setBundleDtlsContext(const std::shared_ptr<BundleContext>& bundleContext,
                                     const DtlsSrtpContext& context);

    SocketPair(const dhtnet::IpAddr& rtpDestAddr,
               const dhtnet::IpAddr& rtcpDestAddr,
               int localRtpPort,
               int localRtcpPort,
               bool rtcpMux = false);
    SocketPair(std::unique_ptr<dhtnet::IceSocket> rtp_sock,
               std::unique_ptr<dhtnet::IceSocket> rtcp_sock,
               bool rtcpMux = false);
    SocketPair(const std::shared_ptr<BundleContext>& bundleContext,
               bool rtcpMux = false,
               std::optional<unsigned> rtpPayloadType = std::nullopt);
    ~SocketPair();

    void interrupt();

    // Set the read blocking mode.
    // By default, the read operation will block until data is available
    // on the socket. This method allows to switch to unblocking mode
    // if to stop the receiver thread to exit and there is no more data
    // to read (if the peer mutes/stops the media/RTP stream).
    void setReadBlockingMode(bool blocking);

    MediaIOHandle* createIOContext(const uint16_t mtu);

    void openSockets(const dhtnet::IpAddr& rtpDestAddr,
                     const dhtnet::IpAddr& rtcpDestAddr,
                     int localRtpPort,
                     int localRtcpPort);
    void closeSockets();
    void setDefaultRemoteAddresses(const dhtnet::IpAddr& rtpDestAddr, const dhtnet::IpAddr& rtcpDestAddr);

    /*
       Supported suites are:

       AES_CM_128_HMAC_SHA1_80
       SRTP_AES128_CM_HMAC_SHA1_80
       AES_CM_128_HMAC_SHA1_32
         SRTP_AES128_CM_HMAC_SHA1_32
         AES_256_CM_HMAC_SHA1_80
         AES_256_CM_HMAC_SHA1_32

       Example (unsecure) usage:
       createSRTP("AES_CM_128_HMAC_SHA1_80",
                  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn",
                  "AES_CM_128_HMAC_SHA1_80",
                  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn");

       Will throw an std::runtime_error on failure, should be handled at a higher level
    */
    void createSRTP(const char* out_suite, const char* out_params, const char* in_suite, const char* in_params);

    /**
     * Enable SRTCP protection of RTCP packets (RFC 3711 3.4) with the keys
     * given to createSRTP(): outgoing RTCP is encrypted and authenticated,
     * incoming RTCP is decrypted (with a plaintext fallback for robustness).
     *
     * This must be enabled when the SRTP keys were negotiated with DTLS-SRTP
     * (RFC 5764), as WebRTC endpoints require protected RTCP. It is kept
     * disabled for SDES sessions where legacy Jami peers exchange RTCP in
     * plaintext.
     */
    void setRtcpProtection(bool enabled) { rtcpProtection_ = enabled; }

    void stopSendOp(bool state = true);
    std::list<rtcpRRHeader> getRtcpRR();
    std::list<rtcpREMBHeader> getRtcpREMB();
    std::list<TransportCcFeedback> getRtcpTransportCc();
    std::list<TransportCcReport> getRtcpTransportCcReports();
    size_t getTransportCcSentPacketCount() const;
    std::vector<uint8_t> createRtcpTransportCcFeedback(uint32_t senderSsrc, uint32_t mediaSsrc);

    /**
     * Build a Picture Loss Indication feedback packet (RFC 4585 6.3.1).
     */
    static std::vector<uint8_t> createRtcpPli(uint32_t senderSsrc, uint32_t mediaSsrc);

    /**
     * Return true if the buffer holds a PSFB keyframe request:
     * PLI (RFC 4585 6.3.1) or FIR (RFC 5104 4.3.1).
     */
    static bool isRtcpKeyframeRequest(const uint8_t* buf, size_t len);

    bool waitForRTCP(std::chrono::milliseconds interval);
    double getLastLatency();

    void setPacketLossCallback(std::function<void(void)> cb) { packetLossCallback_ = std::move(cb); }
    void setKeyframeRequestCallback(std::function<void(void)> cb) { keyframeRequestCallback_ = std::move(cb); }
    void setRtpDelayCallback(std::function<void(int, int)> cb);
    void setBundleMidExtension(std::string localMid,
                               std::optional<unsigned> localMidExtId,
                               std::string remoteMid,
                               std::optional<unsigned> remoteMidExtId);
    void setTransportCcExtension(std::optional<unsigned> localExtId, std::optional<unsigned> remoteExtId);
    void setRtpPacingBitrate(uint64_t bitrateBps);

    int writeData(const uint8_t* buf, int buf_size);
    int writeRtcpData(const uint8_t* buf, int buf_size);

    std::optional<uint32_t> getLocalSsrc() const;
    std::optional<uint32_t> getRemoteSsrc() const;

    uint16_t lastSeqValOut();

private:
    NON_COPYABLE(SocketPair);
    struct PacketState;
    struct TransportCcState;
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    struct CachedRtpPacket
    {
        uint16_t sequence {};
        uint32_t ssrc {};
        time_point sentAt {};
        std::vector<uint8_t> payload {};
    };
    struct NackPackets
    {
        std::vector<std::vector<uint8_t>> packets;
        size_t missed {};
    };

    int readCallback(uint8_t* buf, int buf_size);
    int writeCallback(const uint8_t* buf, int buf_size);

    int waitForData();
    int queueMuxedSocketData();
    int readRtpData(void* buf, int buf_size);
    int readRtcpData(void* buf, int buf_size);
    void queuePacket(std::vector<uint8_t>&& packet, bool isRtcp);
    void setLocalSsrc(uint32_t ssrc);
    void setRemoteSsrc(uint32_t ssrc);
    std::optional<RtpPacerPacket> paceRtpPacket(const uint8_t* buf, int buf_size);
    std::optional<uint16_t> nextTransportCcSequenceNumber();
    void recordTransportCcSend(uint16_t sequenceNumber, size_t packetSize);
    std::optional<TransportCcReport> createTransportCcReport(const TransportCcFeedback& feedback);
    static void recordTransportCcReceive(const std::shared_ptr<PacketState>& packetState,
                                         const uint8_t* buf,
                                         size_t len);
    static bool isRtcpPacket(const uint8_t* buf, size_t len);
    void saveRtcpRRPacket(uint8_t* buf, size_t len);
    void saveRtcpREMBPacket(uint8_t* buf, size_t len);
    void saveRtcpTransportCcPacket(uint8_t* buf, size_t len);
    void cacheSentRtpPacket(const uint8_t* buf, size_t len);
    void retransmitNackPackets(const uint8_t* buf, size_t len);
    static std::vector<uint16_t> parseNackSequences(const uint8_t* buf, size_t len);
    NackPackets findNackPackets(uint32_t mediaSsrc, const std::vector<uint16_t>& requestedSequences);
    void queueNackPacket(std::vector<uint8_t> packet);
    void sendNackPackets();

    dhtnet::IceSocket* getRtpSocket() const;
    dhtnet::IceSocket* getRtcpSocket() const;

    std::shared_ptr<PacketState> packetState_;
    std::shared_ptr<BundleContext> bundleContext_;

    int rtpHandle_ {-1};
    int rtcpHandle_ {-1};
    dhtnet::IpAddr rtpDestAddr_;
    dhtnet::IpAddr rtcpDestAddr_;
    bool rtcpMux_ {false};
    std::atomic_bool noWrite_ {false};
    std::atomic_bool rtcpProtection_ {false};
    std::unique_ptr<SRTPProtoContext> srtpContext_;
    std::mutex srtpWriteMutex_ {};
    std::function<void(void)> packetLossCallback_;
    std::function<void(void)> keyframeRequestCallback_;
    std::function<void(int, int)> rtpDelayCallback_;
    std::optional<unsigned> localRtpMidExtId_ {};
    std::string localRtpMid_ {};
    std::optional<unsigned> localTransportCcExtId_ {};
    std::atomic<uint64_t> rtpPacingBitrateBps_ {0};
    RtpPacer rtpPacer_ {};
    std::mutex rtpPacerMutex_ {};
    std::deque<CachedRtpPacket> sentRtpPackets_ {};
    std::mutex sentRtpPacketsMutex_ {};
    RtpPacer nackPacer_ {};
    std::mutex nackSenderMutex_ {};
    std::condition_variable nackSenderCv_ {};
    std::thread nackSender_ {};
    time_point lastNackKeyframeRequest_ {};
    bool getOneWayDelayGradient(float sendTS, bool marker, int32_t* gradient, int32_t* deltaR);
    bool parse_RTP_ext(uint8_t* buf, float* abs);

    std::list<rtcpRRHeader> listRtcpRRHeader_;
    std::list<rtcpREMBHeader> listRtcpREMBHeader_;
    std::list<TransportCcFeedback> listRtcpTransportCc_;
    std::list<TransportCcReport> listRtcpTransportCcReports_;
    std::mutex rtcpInfo_mutex_;
    std::condition_variable cvRtcpPacketReadyToRead_;
    static constexpr unsigned MAX_LIST_SIZE {10};

    mutable std::atomic_bool rtcpPacketLoss_ {false};
    double lastSRTS_ {};
    uint32_t lastDLSR_ {};

    std::list<double> histoLatency_;

    time_point lastRR_time;
    uint16_t lastSeqNumIn_ {0};
    float lastSendTS_ {0.0f};
    time_point lastReceiveTS_ {};
    time_point arrival_TS {};

    TS_Frame svgTS = {};
};

} // namespace jami
