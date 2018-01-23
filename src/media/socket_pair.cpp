/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
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

#include "ip_utils.h" // MUST BE INCLUDED FIRST
#include "libav_deps.h" // THEN THIS ONE AFTER

#include "socket_pair.h"
#include "ice_socket.h"
#include "libav_utils.h"
#include "logger.h"
#include "security/memory.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <iterator>

extern "C" {
#include "srtp.h"
}

#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/types.h>
#include <ciso646> // fix windows compiler bug

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

static constexpr int NET_POLL_TIMEOUT = 100; /* poll() timeout in ms */
static constexpr int RTP_MAX_PACKET_LENGTH = 2048;
static constexpr auto UDP_HEADER_SIZE = 8;
static constexpr auto SRTP_OVERHEAD = 10;
static constexpr uint32_t RTCP_RR_FRACTION_MASK = 0xFF000000;

enum class DataType : unsigned { RTP=1<<0, RTCP=1<<1 };

class SRTPProtoContext {
public:
    SRTPProtoContext(const char* out_suite, const char* out_key,
                     const char* in_suite, const char* in_key) {
        ring_secure_memzero(&srtp_out, sizeof(srtp_out));
        ring_secure_memzero(&srtp_in, sizeof(srtp_in));
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

static int
udp_socket_create(int family, int port)
{
    int udp_fd = -1;

#ifdef __APPLE__
    udp_fd = socket(family, SOCK_DGRAM, 0);
    if (udp_fd >= 0 && fcntl(udp_fd, F_SETFL, O_NONBLOCK) < 0) {
        close(udp_fd);
        udp_fd = -1;
    }
#elif defined _WIN32
    udp_fd = socket(family, SOCK_DGRAM, 0);
    u_long block = 1;
    if (udp_fd >= 0 && ioctlsocket(udp_fd, FIONBIO, &block) < 0) {
        close(udp_fd);
        udp_fd = -1;
    }
#else
    udp_fd = socket(family, SOCK_DGRAM | SOCK_NONBLOCK, 0);
#endif

    if (udp_fd < 0) {
        RING_ERR("socket() failed");
        strErr();
        return -1;
    }

    auto bind_addr = ip_utils::getAnyHostAddr(family);
    if (not bind_addr.isIpv4() and not bind_addr.isIpv6()) {
        RING_ERR("No IPv4/IPv6 host found for family %u", family);
        close(udp_fd);
        return -1;
    }

    bind_addr.setPort(port);
    RING_DBG("use local address: %s", bind_addr.toString(true, true).c_str());
    if (::bind(udp_fd, bind_addr, bind_addr.getLength()) < 0) {
        RING_ERR("bind() failed");
        strErr();
        close(udp_fd);
        udp_fd = -1;
    }

    return udp_fd;
}

SocketPair::SocketPair(const char *uri, int localPort)
{
    openSockets(uri, localPort);
}

SocketPair::SocketPair(std::unique_ptr<IceSocket> rtp_sock,
                       std::unique_ptr<IceSocket> rtcp_sock)
    : rtp_sock_(std::move(rtp_sock))
    , rtcp_sock_(std::move(rtcp_sock))
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
SocketPair::saveRtcpPacket(uint8_t* buf, size_t len)
{
    if (len < sizeof(rtcpRRHeader))
        return;

    auto header = reinterpret_cast<rtcpRRHeader*>(buf);
    if(header->pt != 201) //201 = RR PT
        return;

    std::lock_guard<std::mutex> lock(rtcpInfo_mutex_);

    if (listRtcpHeader_.size() >= MAX_LIST_SIZE) {
        listRtcpHeader_.pop_front();
    }

    listRtcpHeader_.push_back(*header);
}

std::vector<rtcpRRHeader>
SocketPair::getRtcpInfo()
{
    decltype(listRtcpHeader_) l;
    {
        std::lock_guard<std::mutex> lock(rtcpInfo_mutex_);
        if (listRtcpHeader_.empty())
            return {};
        l = std::move(listRtcpHeader_);
    }
    return {std::make_move_iterator(l.begin()), std::make_move_iterator(l.end())};
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
    int dst_rtp_port;

    libav_utils::ring_url_split(uri, hostname, sizeof(hostname), &dst_rtp_port, path, sizeof(path));

    const int local_rtcp_port = local_rtp_port + 1;
    const int dst_rtcp_port = dst_rtp_port + 1;

    rtpDestAddr_ = IpAddr {hostname}; rtpDestAddr_.setPort(dst_rtp_port);
    rtcpDestAddr_ = IpAddr {hostname}; rtcpDestAddr_.setPort(dst_rtcp_port);

    // Open local sockets (RTP/RTCP)
    if ((rtpHandle_ = udp_socket_create(rtpDestAddr_.getFamily(), local_rtp_port)) == -1 or
        (rtcpHandle_ = udp_socket_create(rtcpDestAddr_.getFamily(), local_rtcp_port)) == -1) {
        closeSockets();
        throw std::runtime_error("Sockets creation failed");
    }

    RING_WARN("SocketPair: local{%d,%d} / %s{%d,%d}",
              local_rtp_port, local_rtcp_port, hostname, dst_rtp_port, dst_rtcp_port);
}

MediaIOHandle*
SocketPair::createIOContext(const uint16_t mtu)
{
    unsigned ip_header_size;
    if (rtp_sock_)
        ip_header_size = rtp_sock_->getTransportOverhead();
    else if (rtpDestAddr_.getFamily() == AF_INET6)
        ip_header_size = 40;
    else
        ip_header_size = 20;
    return new MediaIOHandle( mtu - (srtpContext_ ? SRTP_OVERHEAD : 0) - UDP_HEADER_SIZE - ip_header_size,
                              true,
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
        saveRtcpPacket(buf, len);
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

    return len;
}

int
SocketPair::writeData(uint8_t* buf, int buf_size)
{
    bool isRTCP = RTP_PT_IS_RTCP(buf[1]);

    // System sockets?
    if (rtpHandle_ >= 0) {
        int fd;
        IpAddr* dest_addr;

        if (isRTCP) {
            fd = rtcpHandle_;
            dest_addr = &rtcpDestAddr_;
        } else {
            fd = rtpHandle_;
            dest_addr = &rtpDestAddr_;
        }

        auto ret = ff_network_wait_fd(fd);
        if (ret < 0)
            return ret;

        if (noWrite_)
            return buf_size;
        return ::sendto(fd, reinterpret_cast<const char*>(buf), buf_size, 0,
                        *dest_addr, dest_addr->getLength());
    }

    if (noWrite_)
        return buf_size;

    // IceSocket
    if (isRTCP)
        return rtcp_sock_->send(buf, buf_size);
    else
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

    // check if we're sending an RR, if so, detect packet loss
    // buf_size gives length of buffer, not just header
    if (isRTCP && static_cast<unsigned>(buf_size) >= sizeof(rtcpRRHeader)) {
        auto header = reinterpret_cast<rtcpRRHeader*>(buf);
        rtcpPacketLoss_ = (header->pt == 201 && ntohl(header->fraction_lost) & RTCP_RR_FRACTION_MASK);
    }

    do {
        if (interrupted_)
            return -EINTR;
        ret = writeData(buf, buf_size);
    } while (ret < 0 and errno == EAGAIN);

    return ret < 0 ? -errno : ret;
}

bool
SocketPair::rtcpPacketLossDetected() const
{
    // set to false on checking packet loss to avoid burst of keyframe requests
    bool b = true;
    return rtcpPacketLoss_.compare_exchange_strong(b, false);
}

} // namespace ring
