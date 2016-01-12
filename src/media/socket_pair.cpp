/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
 *  Copyright (c) 2002 Fabrice Bellard
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "socket_pair.h"
#include "ice_socket.h"
#include "libav_utils.h"
#include "logger.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <iterator>
#include <bitset>

extern "C" {
#include "srtp.h"
}

#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/types.h>

#ifdef _WIN32
#define SOCK_NONBLOCK FIONBIO
#define poll WSAPoll
#define close(x) closesocket(x)
#endif

#ifdef __ANDROID__
#include <asm-generic/fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#ifdef __APPLE__
#include <fcntl.h>
#endif

namespace ring {

using clock = std::chrono::steady_clock;

static constexpr int NET_POLL_TIMEOUT = 100; /* poll() timeout in ms */
static constexpr int RTP_MAX_PACKET_LENGTH = 2048;

enum class DataType : unsigned { RTP=1<<0, RTCP=1<<1 };

class SRTPProtoContext {
public:
    SRTPProtoContext(const char* out_suite, const char* out_key,
                     const char* in_suite, const char* in_key) {
        if (out_suite && out_key) {
            // XXX: see srtp_open from libavformat/srtpproto.c
            if (ff_srtp_set_crypto(&srtp_out, out_suite, out_key) < 0) {
                srtp_close();
                throw std::runtime_error("Could not set crypto on output");
            }
        }

        if (in_suite && in_key) {
            if (ff_srtp_set_crypto(&srtp_in, in_suite, in_key) < 0) {
                srtp_close();
                throw std::runtime_error("Could not set crypto on input");
            }
        }
    }

    ~SRTPProtoContext() {
        srtp_close();
    }

    SRTPContext srtp_out {};
    SRTPContext srtp_in {};
    uint8_t encryptbuf[RTP_MAX_PACKET_LENGTH];

private:
    void srtp_close() noexcept {
        ff_srtp_free(&srtp_out);
        ff_srtp_free(&srtp_in);
    }
};

static int
ff_network_wait_fd(int fd)
{
    struct pollfd p = { fd, POLLOUT, 0 };
    auto ret = poll(&p, 1, NET_POLL_TIMEOUT);
    return ret < 0 ? errno : p.revents & (POLLOUT | POLLERR | POLLHUP) ? 0 : -EAGAIN;
}

static struct addrinfo*
udp_resolve_host(const char* node, int service)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    char sport[16];
    snprintf(sport, sizeof(sport), "%d", service);

    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* res = nullptr;
    if (auto error = getaddrinfo(node, sport, &hints, &res)) {
        res = nullptr;
        RING_ERR("getaddrinfo failed: %s\n", gai_strerror(error));
    }

    return res;
}

static unsigned
udp_set_url(struct sockaddr_storage* addr, const char* hostname, int port)
{
    auto res0 = udp_resolve_host(hostname, port);
    if (res0 == 0)
        return 0;
    memcpy(addr, res0->ai_addr, res0->ai_addrlen);
    auto addr_len = res0->ai_addrlen;
    freeaddrinfo(res0);

    return addr_len;
}

static int
udp_socket_create(sockaddr_storage* addr, socklen_t* addr_len, int local_port)
{
    int udp_fd = -1;
    struct addrinfo* res0 = nullptr;
    struct addrinfo* res = nullptr;

    res0 = udp_resolve_host(0, local_port);
    if (res0 == 0)
        return -1;
    for (res = res0; res; res=res->ai_next) {
#ifdef __APPLE__
        udp_fd = socket(res->ai_family, SOCK_DGRAM, 0);
        if (udp_fd != -1 && fcntl(udp_fd, F_SETFL, O_NONBLOCK) != -1)
#else
        udp_fd = socket(res->ai_family, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        if (udp_fd != -1)
#endif
           break;

        RING_ERR("socket error");
     }

    if (udp_fd < 0) {
        freeaddrinfo(res0);
        return -1;
    }

    memcpy(addr, res->ai_addr, res->ai_addrlen);
    *addr_len = res->ai_addrlen;

    // bind socket so that we send from and receive
    // on local port
    if (bind(udp_fd, reinterpret_cast<sockaddr*>(addr), *addr_len) < 0) {
        RING_ERR("Bind failed");
        strErr();
        close(udp_fd);
        udp_fd = -1;
    }

    freeaddrinfo(res0);

    return udp_fd;
}

// Maximal size allowed for a RTP packet, this value of 1460 bytes is PPoE safe.
static const size_t RTP_BUFFER_SIZE = 1460;
static const size_t SRTP_BUFFER_SIZE = RTP_BUFFER_SIZE - 10;

static inline uint8_t rtpPayloadType(const uint8_t* data)
{
    return data[1];
}

template <typename T>
static inline const T*
offsetPtr32(const T* p, unsigned int i)
{
    return reinterpret_cast<const T*>(reinterpret_cast<const uint32_t*>(p) + i);
}

template <std::size_t offset=0, typename T>
static inline uint32_t
read32(const T* data)
{
    return ntohl(reinterpret_cast<const uint32_t*>(data)[offset]);
}

template <typename T>
static inline const T*
unpackRtpBase(const T* data, rtpBase& base)
{
    base.n = read32(data);
    return offsetPtr32(data, 1);
}

template <typename T>
static inline const T*
unpackRtpHead(const T* data, rtpHead& head)
{
    data = unpackRtpBase(data, head.base);
    head.ts = read32<0>(data);
    head.ssrc = read32<1>(data);
    return offsetPtr32(data, 2);
}

template <typename T>
static inline const T*
unpackRtcpReportHead(const T* data, rtcpReportHead& report)
{
    data = unpackRtpBase(data, report.base);
    report.ssrc = read32(data);
    return offsetPtr32(data, 1);
}

SocketPair::RawPacket::RawPacket(const uint8_t* buf, std::size_t len)
{
    reserve(len);
    std::copy_n(buf, len, data());
}

SocketPair::SocketPair(const char *uri, int localPort)
    : rtp_sock_()
    , rtcp_sock_()
    , rtpDestAddr_()
    , rtpDestAddrLen_()
    , rtcpDestAddr_()
    , rtcpDestAddrLen_()
{
    openSockets(uri, localPort);
}

SocketPair::SocketPair(std::unique_ptr<IceSocket> rtp_sock,
                       std::unique_ptr<IceSocket> rtcp_sock)
    : rtp_sock_(std::move(rtp_sock))
    , rtcp_sock_(std::move(rtcp_sock))
    , rtpDestAddr_()
    , rtpDestAddrLen_()
    , rtcpDestAddr_()
    , rtcpDestAddrLen_()
{
    auto queueRtpPacket = [this](uint8_t* buf, size_t len) {
        std::lock_guard<std::mutex> l(dataBuffMutex_);
        rtpDataBuff_.emplace_back(buf, buf+len);
        cv_.notify_one();
        return len;
    };

    auto queueRtcpPacket = [this](uint8_t* buf, size_t len) {
        std::lock_guard<std::mutex> l(dataBuffMutex_);
        rtcpDataBuff_.emplace_back(buf, buf+len);
        cv_.notify_one();
        return len;
    };

    rtp_sock_->setOnRecv(queueRtpPacket);
    rtcp_sock_->setOnRecv(queueRtcpPacket);
}

SocketPair::~SocketPair()
{
    interrupt();
    closeSockets();
}

void
SocketPair::createSRTP(const char* out_suite, const char* out_key,
                       const char* in_suite, const char* in_key)
{
    srtpContext_.reset(new SRTPProtoContext(out_suite, out_key, in_suite, in_key));
}

void
SocketPair::interrupt()
{
    interrupted_ = true;
    if (rtp_sock_) rtp_sock_->setOnRecv(nullptr);
    if (rtcp_sock_) rtcp_sock_->setOnRecv(nullptr);
    cv_.notify_all();
}

void
SocketPair::stopSendOp(bool state)
{
    noWrite_ = state;
}

void
SocketPair::closeSockets()
{
    if (rtcpHandle_ > 0 and close(rtcpHandle_))
        strErr();
    if (rtpHandle_ > 0 and close(rtpHandle_))
        strErr();
}

void
SocketPair::openSockets(const char* uri, int local_rtp_port)
{
    char hostname[256];
    char path[1024];
    int rtp_port;

    libav_utils::ring_url_split(uri, hostname, sizeof(hostname), &rtp_port, path, sizeof(path));

    const int rtcp_port = rtp_port + 1;
    const int local_rtcp_port = local_rtp_port + 1;

    sockaddr_storage rtp_addr, rtcp_addr;
    socklen_t rtp_len, rtcp_len;

    // Open sockets and store addresses for sending
    if ((rtpHandle_ = udp_socket_create(&rtp_addr, &rtp_len, local_rtp_port)) == -1 or
        (rtcpHandle_ = udp_socket_create(&rtcp_addr, &rtcp_len, local_rtcp_port)) == -1 or
        (rtpDestAddrLen_ = udp_set_url(&rtpDestAddr_, hostname, rtp_port)) == 0 or
        (rtcpDestAddrLen_ = udp_set_url(&rtcpDestAddr_, hostname, rtcp_port)) == 0) {

        // Handle failed socket creation
        closeSockets();
        throw std::runtime_error("Socket creation failed");
    }

    RING_WARN("SocketPair: local{%d,%d} / %s{%d,%d}",
              local_rtp_port, local_rtcp_port, hostname, rtp_port, rtcp_port);
}

MediaIOHandle*
SocketPair::createIOContext()
{
    return new MediaIOHandle(srtpContext_ ? SRTP_BUFFER_SIZE : RTP_BUFFER_SIZE, true,
                             [](void* sp, uint8_t* buf, int len){ return static_cast<SocketPair*>(sp)->readCallback(buf, len); },
                             [](void* sp, uint8_t* buf, int len){ return static_cast<SocketPair*>(sp)->writeCallback(buf, len); },
                             0, reinterpret_cast<void*>(this));
}

int
SocketPair::waitForData()
{
    // System sockets
    if (rtpHandle_ >= 0) {
        int ret;
        do {
            if (interrupted_) {
                errno = EINTR;
                return -1;
            }

            // work with system socket
            struct pollfd p[2] = { {rtpHandle_, POLLIN, 0},
                                   {rtcpHandle_, POLLIN, 0} };
            ret = poll(p, 2, NET_POLL_TIMEOUT);
            if (ret > 0) {
                ret = 0;
                if (p[0].revents & POLLIN)
                    ret |= static_cast<int>(DataType::RTP);
                if (p[1].revents & POLLIN)
                    ret |= static_cast<int>(DataType::RTCP);
            }
        } while (!ret or (ret < 0 and errno == EAGAIN));

        return ret;

    }

    // work with IceSocket
    {
        std::unique_lock<std::mutex> lk(dataBuffMutex_);
        cv_.wait(lk, [this]{ return interrupted_ or not rtpDataBuff_.empty() or not rtcpDataBuff_.empty(); });
    }

    if (interrupted_) {
        errno = EINTR;
        return -1;
    }

    return static_cast<int>(DataType::RTP) | static_cast<int>(DataType::RTCP);
}

int
SocketPair::readRtpData(void* buf, int buf_size)
{
    // handle system socket
    if (rtpHandle_ >= 0) {
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);
        return recvfrom(rtpHandle_, static_cast<char*>(buf), buf_size, 0,
                        reinterpret_cast<struct sockaddr*>(&from), &from_len);
    }

    // handle ICE
    std::unique_lock<std::mutex> lk(dataBuffMutex_);
    if (not rtpDataBuff_.empty()) {
        auto pkt = rtpDataBuff_.front();
        rtpDataBuff_.pop_front();
        lk.unlock(); // to not block our ICE callbacks
        int pkt_size = pkt.size();
        int len = std::min(pkt_size, buf_size);
        std::copy_n(pkt.begin(), len, static_cast<char*>(buf));
        return len;
    }

    return 0;
}

int
SocketPair::readRtcpData(void* buf, int buf_size)
{
    // handle system socket
    if (rtcpHandle_ >= 0) {
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);
        return recvfrom(rtcpHandle_, static_cast<char*>(buf), buf_size, 0,
                        reinterpret_cast<struct sockaddr*>(&from), &from_len);
    }

    // handle ICE
    std::unique_lock<std::mutex> lk(dataBuffMutex_);
    if (not rtcpDataBuff_.empty()) {
        auto pkt = rtcpDataBuff_.front();
        rtcpDataBuff_.pop_front();
        lk.unlock();
        int pkt_size = pkt.size();
        int len = std::min(pkt_size, buf_size);
        std::copy_n(pkt.begin(), len, static_cast<char*>(buf));
        return len;
    }

    return 0;
}

int
SocketPair::readCallback(uint8_t* buf, int buf_size)
{
    auto datatype = waitForData();
    if (datatype < 0)
        return datatype;

    int len = 0;
    bool fromRTCP = false;

    // Priority to RTCP as its less invasive in bandwidth
    if (datatype & static_cast<int>(DataType::RTCP)) {
        len = readRtcpData(buf, buf_size);
        fromRTCP = true;
    }

    // No RTCP... try RTP
    if (!len and (datatype & static_cast<int>(DataType::RTP))) {
        len = readRtpData(buf, buf_size);
        fromRTCP = false;
    }

    if (len <= 0)
        return len;

    // SRTP decrypt
    if (not fromRTCP and srtpContext_ and srtpContext_->srtp_in.aes) {
        auto err = ff_srtp_decrypt(&srtpContext_->srtp_in, buf, &len);
        if (err < 0)
            RING_WARN("decrypt error %d", err);
    }

    // RTP or RTCP?
    if (not RTP_PT_IS_RTCP(buf[1]))
        analyseRawRTPPacket(buf, len);
    else
        saveRawRtcpPacket(buf, len);

    return len;
}

int
SocketPair::writeData(uint8_t* buf, int buf_size)
{
    if (noWrite_)
        return buf_size;

    // RTCP packet are dropped: we handle RTCP reports ourself
    if (RTP_PT_IS_RTCP(buf[1]))
        return buf_size;

    // System sockets?
    if (rtpHandle_ >= 0) {
        auto ret = ff_network_wait_fd(rtpHandle_);
        if (ret < 0)
            return ret;

        return sendto(rtpHandle_, reinterpret_cast<const char*>(buf), buf_size, 0,
                      reinterpret_cast<sockaddr*>(&rtpDestAddr_), rtpDestAddrLen_);
    }

    // else IceSocket
    return rtp_sock_->send(buf, buf_size);
}

int
SocketPair::writeCallback(uint8_t* buf, int buf_size)
{
    int ret;
    bool isRTCP = RTP_PT_IS_RTCP(buf[1]);

    // Encrypt?
    if (not isRTCP and srtpContext_ and srtpContext_->srtp_out.aes) {
        buf_size = ff_srtp_encrypt(&srtpContext_->srtp_out, buf,
                                   buf_size, srtpContext_->encryptbuf,
                                   sizeof(srtpContext_->encryptbuf));
        if (buf_size < 0) {
            RING_WARN("encrypt error %d", buf_size);
            return buf_size;
        }

        buf = srtpContext_->encryptbuf;
    }

    do {
        if (interrupted_)
            return -EINTR;
        ret = writeData(buf, buf_size);
    } while (ret < 0 and errno == EAGAIN);

    return ret < 0 ? -errno : ret;
}

void
SocketPair::saveRawRtcpPacket(const uint8_t* buf, std::size_t len)
{
    // drop empty report
    if (len <= sizeof(rtcpReportHead))
        return;

    // drop non-RR packet
    auto pt = rtpPayloadType(buf);
    if(pt != 201)
        return;

    std::lock_guard<std::mutex> lk {rtcpPacketMutex_};
    if (rtcpPacketQueue_.size() >= MAX_RTCP_PACKETS)
        rtcpPacketQueue_.pop();
    rtcpPacketQueue_.emplace(buf, len);
}

std::vector<std::vector<uint8_t>>
SocketPair::getRawRtcpPackets()
{
    std::lock_guard<std::mutex> lk {rtcpPacketMutex_};
    std::vector<std::vector<uint8_t>> results;
    while (rtcpPacketQueue_.size()) {
        results.push_back(std::move(rtcpPacketQueue_.front()));
        rtcpPacketQueue_.pop();
    }
    return results;
}

static constexpr unsigned RTP_SEQ_MOD {1<<16};
static constexpr unsigned MIN_SEQUENTIAL {2};
static constexpr unsigned GMIN {16};

// Per-source statistics
struct RtpStats {
    RtpStats(uint32_t ssrc, uint16_t seq) : ssrc(ssrc) {
        reset(seq);
        max_seq = seq - 1;
        probation = MIN_SEQUENTIAL;
    }

    inline void reset(uint16_t seq) {
        base_seq = seq;
        max_seq = seq;
        bad_seq = {RTP_SEQ_MOD + 1};
        cycles = 0;
        received = 0;
        expected_prior = 0;
        received_prior = 0;
    }

    int checkSeq(uint16_t seq);
    void resetInterval();
    void onGoodPacket(uint16_t seq);
    void onLostPacket();
    void dumpInterval(uint32_t ssrc, uint8_t pt);

    uint32_t ssrc {0};
    bool was_valid {false};
    uint32_t total_lost {0};

    // from RFC 3550
    uint32_t base_seq;          /* base seq number */
    uint16_t max_seq;           /* highest seq. number seen */
    uint32_t probation;         /* sequ. packets till source is valid */
    uint32_t bad_seq;           /* last 'bad' seq number + 1 */
    uint32_t cycles;            /* shifted count of seq. number cycles */
    uint32_t received;          /* packets received */
    uint32_t expected_prior;    /* packet expected at last interval */
    uint32_t received_prior;    /* packet received at last interval */
    //uint32_t transit; /* relative trans time for prev pkt */
    //uint32_t jitter; /* estimated jitter */

    // from RFC 3611
    uint32_t loss_count;
    uint32_t lost;
    uint32_t rx_since_loss;
    uint32_t c11;
    uint32_t c13;
    uint32_t c14;
    uint32_t c22;
    uint32_t c23;
    uint32_t c33;
};

// This function is mostly a copy of code from RFC 3550, ch A.1
int
RtpStats::checkSeq(uint16_t seq)
{
    static constexpr unsigned MAX_DROPOUT {3000}; // accepted positive distance from max_seq
    static constexpr unsigned MAX_MISORDER {100}; // accepted negative distance from max_seq


    // Source is not valid until MIN_SEQUENTIAL packets with
    // sequential sequence numbers have been received.
    if (probation) {
        // packet is in sequence
        if (seq == max_seq + 1) {
            max_seq = seq;
            if (--probation == 0) {
                reset(seq);
                ++received;
                return 1;
            }
        } else {
            probation = MIN_SEQUENTIAL - 1;
            max_seq = seq;
        }
        return 0;
    }

    uint16_t udelta = seq - max_seq;

    // in order, with permissible gap?
    if (udelta < MAX_DROPOUT) {
        // Sequence number wrapped?
        if (seq < max_seq) {
            // count another 64K cycle
            cycles += RTP_SEQ_MOD;
        }
        max_seq = seq;
    } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
        // the sequence number made a very large jump
        if (seq == bad_seq) {
            // Two sequential packets -- assume that the other side
            // restarted without telling us so just re-sync
            // (i.e., pretend this was the first packet).
            reset(seq);
        } else {
            bad_seq = (seq + 1) & (RTP_SEQ_MOD-1);
            return 0;
        }
    } else {
        // duplicate or reordered packet
    }
    ++received;
    return 1;
}

void
RtpStats::resetInterval()
{
    loss_count = 0;
    lost = 0;
    rx_since_loss = 0;
    c11 = c13 = c14 = c22 = c23 = c33 = 0;
}

void
RtpStats::onGoodPacket(uint16_t seq)
{
    ++rx_since_loss;

}

void
RtpStats::onLostPacket()
{
    ++loss_count;
    if (rx_since_loss >= GMIN) {
        if (lost == 1)
            ++c14;
        else
            ++c13;
        lost = 1;
        c11 += rx_since_loss;
    } else {
        ++lost;
        if (rx_since_loss == 0) {
            ++c33;
        } else {
            ++c23;
            c22 += rx_since_loss - 1;
        }
    }
    rx_since_loss = 0;
}

void
RtpStats::dumpInterval(uint32_t ssrc, uint8_t pt)
{
    auto c31 = c13;
    auto c32 = c23;
    auto ctotal = c11 + c14 + c13 + c22 + c23 + c31 + c32 + c33;
    if (ctotal == 0) {
        RING_DBG("no losses");
        return;
    }

    // Calculate burst and gap densities
    float burst_density, gap_density, loss_rate;

    if (auto total_burst = c31 + c32 + c33) {
        float p23;
        auto p32 = (float)c32 / total_burst;
        auto burst_rx_count = c22 + c23;
        if (burst_rx_count)
            p23 = 1 - ((float)c22 / burst_rx_count);
        else
            p23 = 1;
        burst_density = p23 / (p23 + p32); // p23+p32=0 only if c23=0 and c22!=0
    } else
        burst_density = 0;

    if (auto gap_count = c11 + c14)
        gap_density = (float)c14 / gap_count;
    else
        gap_density = 0;

    loss_rate = (float)loss_count / ctotal;

    RING_DBG("[rtp@%08x,%u] States: %u, %u, %u, %u, %u, %u", ssrc, pt,
              c11, c13, c14, c22, c23, c33);
    RING_WARN("[rtp@%08x,%u] ctotal=%u, burst_density=%.3f, gap_density=%.3f, loss_rate=%.3f", ssrc, pt, ctotal,
              burst_density, gap_density, loss_rate);
    resetInterval();
}

void
SocketPair::analyseRawRTPPacket(const uint8_t* buf, std::size_t len)
{
    if (len < sizeof(rtpHead))
        return;

    lastRtpRxTime_ = clock::now();

    rtpHead rtp_head;
    unpackRtpHead(buf, rtp_head);

    const auto ssrc = rtp_head.ssrc;
    const auto pt = rtp_head.base.s.m_pt & ~0x80; // ignore M field

    // TESTING ONLY (VIDEO)
    if (pt != 96)
        return;

    uint16_t seq = rtp_head.base.s.seq_len;

    // Begin analyses?
    if (!rtpStats_ or rtpStats_->ssrc != ssrc) {
        rtpStats_.reset(new RtpStats {rtp_head.ssrc, seq});
        lastRtpStatDump_ = lastRtpRxTime_;
        RING_WARN("[rtp@%08x,%u] new source (seq=%u)", ssrc, pt, seq);
        return;
    }

    auto valid = rtpStats_->checkSeq(seq);
    if (valid) {
        // start of valid packet sequence?
        if (not rtpStats_->was_valid) {
            rtpStats_->was_valid = true;
            rtpStats_->resetInterval();
        }

        auto extended_max = rtpStats_->cycles + rtpStats_->max_seq;
        auto expected = extended_max - rtpStats_->base_seq + 1;
        rtpStats_->total_lost = expected - rtpStats_->received;

        auto expected_interval = expected - rtpStats_->expected_prior;
        rtpStats_->expected_prior = expected;
        auto received_interval = rtpStats_->received - rtpStats_->received_prior;
        rtpStats_->received_prior = rtpStats_->received;

        // Update statistic state machine, packet loss first
        if (auto lost_interval = expected_interval - received_interval) {
            RING_WARN("[rtp@%08x,%u] lost=%u", ssrc, pt, lost_interval);
            for (unsigned i=0; i < lost_interval; ++i)
                rtpStats_->onLostPacket();
        }
        rtpStats_->onGoodPacket(seq);
    } else
        rtpStats_->was_valid = false;

    // periodic task (each 5 seconds)
    if (std::chrono::duration_cast<std::chrono::seconds>(lastRtpRxTime_ - lastRtpStatDump_).count() >= 5) {
        rtpStats_->dumpInterval(ssrc, pt);
        lastRtpStatDump_ = lastRtpRxTime_;
    }
}

#if 0
void
SocketPair::addContentToSerializedQueue(uint16_t content)
{
    uint8_t unsigned8Val;
    unsigned8Val = (uint8_t)(content >> 8);
    rtcpXRPacket_.blkContent.push_back(unsigned8Val);
    unsigned8Val = (uint8_t) content;
    rtcpXRPacket_.blkContent.push_back(unsigned8Val);
}

void
SocketPair::addSeqToBlkContent(bool isLost)
{
    blkContent_.set(XR_BLOCK_CONTENT_MAX - blkContentIndex_, (isLost ? 0 : 1));
    blkContentIndex_++;
    if (blkContentIndex_ > XR_BLOCK_CONTENT_MAX) {
        // set first bit (15) to 1 to identify BLE
        blkContent_.set(15,1);
        addContentToSerializedQueue(blkContent_.to_ulong());
        blkContentIndex_ = 0;
        blkContent_.reset();
    }
    if (isLost) {
        packetDropped_++;
    } else {
        packetReceived_++;
    }
}

void
SocketPair::analyseRTP(const rtpHeader& header)
{
    auto pkt_ssrc = ntohl(header.ssrc);
    auto pkt_seq = ntohs(header.seq);

    if (ssrc + 1 != rtcpXRPacket_.xrHeader.ssrc) {
        //RING_ERR("new SSRC");
        /* TODO:
         * 1- Send RTCP report
         * 2- Flush stats
        */
    }

    if (newRtcpReport_) { //first Packet : fill header
        rtcpXRPacket_.xrHeader.ssrc = ssrc + 1;
        rtcpXRPacket_.blkHeader.beginSeq = header->seq;
        lastSeq_ = currentSeq;
        //first packet is never lost
        addSeqToBlkContent(not PACKET_LOST);
        newRtcpReport_ = false;
    } else {
        auto diff = currentSeq - lastSeq_;
        //TODO: manage wraparound
        if (diff == 0) {
            packetDuplicated_++;
        } else if (diff == 1) {
            //no packet loss -> 1
            addSeqToBlkContent(not PACKET_LOST);
        } else {
            //find if packets arrived lately
            auto it = find (missingCseq_.begin(), missingCseq_.end(), currentSeq);
            if (it != missingCseq_.end()) {
                RING_ERR("seq %u finally arrived !", currentSeq);
                missingCseq_.erase(it);
                packetDropped_--;
            } else {
                //if not add it to list
                auto i = 1;
                while ( i < diff) {
                    missingCseq_.push_back(lastSeq_+i);
                    addSeqToBlkContent(PACKET_LOST);
                    i++;
                }
            }
            // add the packet received
            addSeqToBlkContent(not PACKET_LOST);
        }
    }

    auto rtpCheckTimer = std::chrono::duration_cast<std::chrono::seconds>
        (clock::now() - lastRTPCheck_);

    if (rtpCheckTimer.count() >= RTP_STAT_INTERVAL) {
        sendRTCPXR();
        lastRTPCheck_ = clock::now();
    }
    lastSeq_ = currentSeq;
}

void
SocketPair::sendRTCPXR()
{

    if (blkContentIndex_ != 0) {
        // append last chunk (not full)
        blkContent_.set(15,1);
        addContentToSerializedQueue(blkContent_.to_ulong());

        blkContent_.reset();
        addContentToSerializedQueue(blkContent_.to_ulong());
    }

#ifdef DUMP_STAT
    dumpRTPStats();
#endif

    //header
    rtcpXRPacket_.xrHeader.pt = 207;
    rtcpXRPacket_.xrHeader.version = 2;
    rtcpXRPacket_.xrHeader.ssrc = htonl(rtcpXRPacket_.xrHeader.ssrc);

    //block header
    rtcpXRPacket_.blkHeader.bt = 1;
    rtcpXRPacket_.blkHeader.ssrc = rtcpXRPacket_.xrHeader.ssrc;
    rtcpXRPacket_.blkHeader.endSeq = htons(lastSeq_);


    //length
    auto pktLength = sizeof(rtcpXRHeader) + sizeof(rtcpXRBlockHeader) + rtcpXRPacket_.blkContent.size();

    rtcpXRPacket_.xrHeader.len = htons(pktLength);

    rtcpXRPacket_.blkHeader.len = htons(sizeof(rtcpXRBlockHeader) + rtcpXRPacket_.blkContent.size());

    //serialisation in packet for socket
    uint8_t* packet = (uint8_t*) malloc(sizeof(rtcpXRHeader) + sizeof(rtcpXRBlockHeader) + rtcpXRPacket_.blkContent.size());
    uint8_t* ptr_packet = packet;

    memcpy(ptr_packet, &rtcpXRPacket_.xrHeader, sizeof(rtcpXRHeader));
    ptr_packet += sizeof(rtcpXRHeader);

    memcpy(ptr_packet, &rtcpXRPacket_.blkHeader, sizeof(rtcpXRBlockHeader));
    ptr_packet += sizeof(rtcpXRBlockHeader);

    memcpy(ptr_packet, rtcpXRPacket_.blkContent.data(), rtcpXRPacket_.blkContent.size());

    auto ret = rtcp_sock_->send(packet,pktLength);

    //clear all stats
    rtcpXRPacket_.blkContent.clear();
    missingCseq_.clear();
    packetDropped_ = 0;
    packetReceived_ = 0;
    packetDuplicated_ = 0;
    blkContentIndex_ = 0;
    newRtcpReport_ = true;
}

#ifdef DUMP_STAT
void
SocketPair::dumpRTPStats()
{
    auto beginSeq = ntohs(rtcpXRPacket_.blkHeader.beginSeq);

    RING_ERR("---------------------------------------");
    RING_ERR("-->SEQ %u to %u",
            beginSeq, lastSeq_);
    RING_ERR("-->DROP=%u/%u",
            packetDropped_,
            (lastSeq_ - beginSeq > 0) ? lastSeq_ - beginSeq + 2 : 0);
    RING_ERR("-->RECEIVED=%u",
            packetReceived_);
    RING_ERR("-->DUPLICATED=%u",
            packetDuplicated_);

    if ( not missingCseq_.empty()) {
        RING_ERR("-> Missing cseq (for sure)");
        for (const auto& itCseq : missingCseq_)
            RING_ERR("%u",itCseq);
    }

    //if (packetDropped_ != 0) {
        RING_ERR("-> Detail");
        std::bitset<8> contentBitset;
        for (const auto& itPacket :rtcpXRPacket_.blkContent) {
            contentBitset = std::bitset<8>(itPacket);
            RING_ERR("%s", contentBitset.to_string().c_str());
        }
    //}
    RING_ERR("---------------------------------------");
}
#endif

#endif

} // namespace ring
