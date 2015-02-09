/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Copyright (c) 2002 Fabrice Bellard
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

#include "libav_deps.h"
#include "socket_pair.h"
#include "ice_socket.h"
#include "libav_utils.h"
#include "logger.h"

extern "C" {
#include "srtp.h"
}

#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef __ANDROID__
#include <asm-generic/fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#ifdef __APPLE__
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

namespace ring {

static const int NET_POLL_TIMEOUT = 100; /* poll() timeout in ms */

static const int RTP_MAX_PACKET_LENGTH = 8192;

struct SRTPProtoContext {
    SRTPProtoContext(const char *out_suite, const char *out_key,
                     const char *in_suite, const char *in_key);
    ~SRTPProtoContext();
    void srtp_close();
    SRTPContext srtp_out = {0};
    SRTPContext srtp_in = {0};
    uint8_t encryptbuf[RTP_MAX_PACKET_LENGTH];
};

void SRTPProtoContext::srtp_close()
{
    ff_srtp_free(&srtp_out);
    ff_srtp_free(&srtp_in);
}

// XXX: see srtp_open from libavformat/srtpproto.c
SRTPProtoContext::SRTPProtoContext(const char *out_suite, const char *out_key,
                                   const char *in_suite, const char *in_key)
{
    RING_WARN("Outsuite %s %s", out_suite, out_key);
    RING_WARN("Insuite %s %s", in_suite, in_key);
    if (out_suite && out_key) {
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

SRTPProtoContext::~SRTPProtoContext()
{
    srtp_close();
}

static int
ff_network_wait_fd(int fd)
{
    struct pollfd p = { fd, POLLOUT, 0 };
    int ret;
    ret = poll(&p, 1, NET_POLL_TIMEOUT);
    return ret < 0 ? errno : p.revents & (POLLOUT | POLLERR | POLLHUP) ? 0 : -EAGAIN;
}

static struct
addrinfo* udp_resolve_host(const char *node, int service)
{
    struct addrinfo hints, *res = 0;
    int error;
    char sport[16];

    memset(&hints, 0, sizeof(hints));
    snprintf(sport, sizeof(sport), "%d", service);

    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    if ((error = getaddrinfo(node, sport, &hints, &res))) {
        res = NULL;
        RING_ERR("%s\n", gai_strerror(error));
    }

    return res;
}

static unsigned
udp_set_url(struct sockaddr_storage *addr, const char *hostname, int port)
{
    struct addrinfo *res0;
    int addr_len;

    res0 = udp_resolve_host(hostname, port);
    if (res0 == 0) return 0;
    memcpy(addr, res0->ai_addr, res0->ai_addrlen);
    addr_len = res0->ai_addrlen;
    freeaddrinfo(res0);

    return addr_len;
}

static int
udp_socket_create(sockaddr_storage *addr, socklen_t *addr_len, int local_port)
{
    int udp_fd = -1;
    struct addrinfo *res0 = NULL, *res = NULL;

    res0 = udp_resolve_host(0, local_port);
    if (res0 == 0)
        return -1;
    for (res = res0; res; res=res->ai_next) {
        udp_fd = socket(res->ai_family, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        if (udp_fd != -1) break;
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

using std::string;
static const int RTP_BUFFER_SIZE = 1472;

SocketPair::SocketPair(const char *uri, int localPort)
    : rtp_sock_()
    , rtcp_sock_()
    , rtcpWriteMutex_()
    , rtpDestAddr_()
    , rtpDestAddrLen_()
    , rtcpDestAddr_()
    , rtcpDestAddrLen_()
    , srtpContext_()
{
    openSockets(uri, localPort);
}

SocketPair::SocketPair(std::unique_ptr<IceSocket> rtp_sock,
                       std::unique_ptr<IceSocket> rtcp_sock)
    : rtp_sock_(std::move(rtp_sock))
    , rtcp_sock_(std::move(rtcp_sock))
    , rtcpWriteMutex_()
    , rtpDestAddr_()
    , rtpDestAddrLen_()
    , rtcpDestAddr_()
    , rtcpDestAddrLen_()
    , srtpContext_()
{}

SocketPair::~SocketPair()
{
    interrupted_ = true;
    if (rtpHandle_ >= 0)
        closeSockets();
}

void SocketPair::createSRTP(const char *out_suite, const char *out_key,
                            const char *in_suite, const char *in_key)
{
    srtpContext_.reset(new SRTPProtoContext(out_suite, out_key, in_suite, in_key));
}


void SocketPair::interrupt()
{
    interrupted_ = true;
}

void SocketPair::closeSockets()
{
    if (rtcpHandle_ > 0 and close(rtcpHandle_))
        strErr();
    if (rtpHandle_ > 0 and close(rtpHandle_))
        strErr();
}

void SocketPair::openSockets(const char *uri, int local_rtp_port)
{
    char hostname[256];
    char path[1024];
    int rtp_port;

    libav_utils::sfl_url_split(uri, hostname, sizeof(hostname), &rtp_port, path,
                  sizeof(path));

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

    RING_WARN("SocketPair: local{%d,%d}, remote{%d,%d}",
             local_rtp_port, local_rtcp_port, rtp_port, rtcp_port);
}

MediaIOHandle* SocketPair::createIOContext()
{
    return new MediaIOHandle(RTP_BUFFER_SIZE, true,
                             &readCallback, &writeCallback, 0,
                             reinterpret_cast<void*>(this));
}

int
SocketPair::waitForData()
{
    if (rtpHandle_ >= 0) {
        // work with system socket
        struct pollfd p[2] = { {rtpHandle_, POLLIN, 0},
                               {rtcpHandle_, POLLIN, 0} };
        return poll(p, 2, NET_POLL_TIMEOUT);
    }

    // work with IceSocket
    auto result = rtp_sock_->waitForData(NET_POLL_TIMEOUT);
    if (result < 0) {
        errno = EIO;
        return -1;
    }

    return result;
}

int
SocketPair::readRtpData(void *buf, int buf_size)
{
    auto data = static_cast<uint8_t *>(buf);

    if (rtpHandle_ >= 0) {
        // work with system socket
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);

// XXX: see libavformat/srtpproto.c
start:
        int result = recvfrom(rtpHandle_, buf, buf_size, 0,
                              (struct sockaddr *)&from, &from_len);
        if (result > 0 and srtpContext_ and srtpContext_->srtp_in.aes)
            if (ff_srtp_decrypt(&srtpContext_->srtp_in, data, &result) < 0)
                goto start;

        return result;
    }

    // work with IceSocket
start_ice:
    int result = rtp_sock_->recv(static_cast<unsigned char*>(buf), buf_size);
    if (result > 0 and srtpContext_ and srtpContext_->srtp_in.aes) {
        if (ff_srtp_decrypt(&srtpContext_->srtp_in, data, &result) < 0)
            goto start_ice;
    } else if (result < 0) {
        errno = EIO;
        return -1;
    } else if (result == 0) {
        errno = EAGAIN;
        return -1;
    }

    return result;
}

int
SocketPair::readRtcpData(void *buf, int buf_size)
{
    if (rtcpHandle_ >= 0) {
        // work with system socket
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);
        return recvfrom(rtcpHandle_, buf, buf_size, 0,
                        (struct sockaddr *)&from, &from_len);
    }

    // work with IceSocket
    auto result = rtcp_sock_->recv(static_cast<unsigned char*>(buf), buf_size);
    if (result < 0) {
        errno = EIO;
        return -1;
    }
    if (result == 0) {
        errno = EAGAIN;
        return -1;
    }
    return result;
}

int
SocketPair::writeRtpData(void *buf, int buf_size)
{
    auto data = static_cast<const uint8_t *>(buf);

    if (rtpHandle_ >= 0) {
        auto ret = ff_network_wait_fd(rtpHandle_);
        if (ret < 0)
            return ret;

        if (srtpContext_ and srtpContext_->srtp_out.aes) {
            buf_size = ff_srtp_encrypt(&srtpContext_->srtp_out, data,
                                       buf_size, srtpContext_->encryptbuf,
                                       sizeof(srtpContext_->encryptbuf));
            if (buf_size < 0)
                return buf_size;

            return sendto(rtpHandle_, srtpContext_->encryptbuf, buf_size, 0,
                          (sockaddr*) &rtpDestAddr_, rtpDestAddrLen_);
        }

        return sendto(rtpHandle_, buf, buf_size, 0,
                      (sockaddr*) &rtpDestAddr_, rtpDestAddrLen_);
    }

    // work with IceSocket
    if (srtpContext_ and srtpContext_->srtp_out.aes) {
        buf_size = ff_srtp_encrypt(&srtpContext_->srtp_out, data,
                                   buf_size, srtpContext_->encryptbuf,
                                   sizeof(srtpContext_->encryptbuf));
        if (buf_size < 0)
            return buf_size;

        return rtp_sock_->send(static_cast<unsigned char*>(srtpContext_->encryptbuf), buf_size);
    }

    return rtp_sock_->send(static_cast<unsigned char*>(buf), buf_size);
}

int
SocketPair::writeRtcpData(void *buf, int buf_size)
{
    std::lock_guard<std::mutex> lock(rtcpWriteMutex_);

    if (rtcpHandle_ >= 0) {
        auto ret = ff_network_wait_fd(rtcpHandle_);
        if (ret < 0)
            return ret;
        return sendto(rtcpHandle_, buf, buf_size, 0,
                      (sockaddr*) &rtcpDestAddr_, rtcpDestAddrLen_);
    }

    // work with IceSocket
    return rtcp_sock_->send(static_cast<unsigned char*>(buf), buf_size);
}

int SocketPair::readCallback(void *opaque, uint8_t *buf, int buf_size)
{
    SocketPair *context = static_cast<SocketPair*>(opaque);

retry:
    if (context->interrupted_) {
        RING_ERR("interrupted");
        return -EINTR;
    }

    if (context->waitForData() < 0) {
        if (errno == EINTR)
            goto retry;
        return -EIO;
    }

    /* RTP */
    int len = context->readRtpData(buf, buf_size);
    if (len < 0) {
        if (errno == EAGAIN or errno == EINTR)
            goto retry;
        return -EIO;
    }

    return len;
}

int SocketPair::writeCallback(void *opaque, uint8_t *buf, int buf_size)
{
    SocketPair *context = static_cast<SocketPair*>(opaque);
    int ret;

retry:
    if (RTP_PT_IS_RTCP(buf[1])) {
        return buf_size;
        /* RTCP payload type */
        ret = context->writeRtcpData(buf, buf_size);
        if (ret < 0) {
            if (ret == -EAGAIN)
                goto retry;
            return ret;
        }
    } else {
        /* RTP payload type */
        ret = context->writeRtpData(buf, buf_size);
        if (ret < 0) {
            if (ret == -EAGAIN)
                goto retry;
            return ret;
        }
    }

    return ret < 0 ? errno : ret;
}

} // namespace ring
