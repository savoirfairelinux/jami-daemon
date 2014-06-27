/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include "libav_utils.h"
#include "logger.h"

#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#if defined(__ANDROID__) && !defined(SOCK_NONBLOCK)
#include <asm-generic/fcntl.h>
# define SOCK_NONBLOCK O_NONBLOCK
#endif

static const int NET_POLL_TIMEOUT = 100; /* poll() timeout in ms */

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
        ERROR("%s\n", gai_strerror(error));
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
        ERROR("socket error");
    }

    if (udp_fd < 0) {
        freeaddrinfo(res0);
        return -1;
    }

    memcpy(addr, res->ai_addr, res->ai_addrlen);
    *addr_len = res->ai_addrlen;

#if HAVE_SDP_CUSTOM_IO
    // bind socket so that we send from and receive
    // on local port
    if (bind(udp_fd, reinterpret_cast<sockaddr*>(addr), *addr_len) < 0) {
        ERROR("Bind failed");
        strErr();
        close(udp_fd);
        udp_fd = -1;
    }
#endif

    freeaddrinfo(res0);

    return udp_fd;
}

namespace sfl_video {

static const int RTP_BUFFER_SIZE = 1472;

SocketPair::SocketPair(const char *uri, int localPort) :
           rtcpWriteMutex_(),
           rtpHandle_(0),
           rtcpHandle_(0),
           rtpDestAddr_(),
           rtpDestAddrLen_(),
           rtcpDestAddr_(),
           rtcpDestAddrLen_(),
           interrupted_(false)
{
    openSockets(uri, localPort);
}

SocketPair::~SocketPair()
{
    interrupted_ = true;
    closeSockets();
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

#if HAVE_SDP_CUSTOM_IO
    const int local_rtcp_port = local_rtp_port + 1;
#else
    WARN("libavformat too old for socket reuse, using random source ports");
    local_rtp_port = 0;
    const int local_rtcp_port = 0;
#endif

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
}

VideoIOHandle* SocketPair::createIOContext()
{
    return new VideoIOHandle(RTP_BUFFER_SIZE, true,
                             &readCallback, &writeCallback, 0,
                             reinterpret_cast<void*>(this));
}

int SocketPair::readCallback(void *opaque, uint8_t *buf, int buf_size)
{
    SocketPair *context = static_cast<SocketPair*>(opaque);

    struct sockaddr_storage from;
    socklen_t from_len;
    int len, n;
    struct pollfd p[2] = { {context->rtpHandle_, POLLIN, 0},
						   {context->rtcpHandle_, POLLIN, 0}};

    for(;;) {
        if (context->interrupted_) {
            ERROR("interrupted");
            return -EINTR;
        }

        /* build fdset to listen to RTP and RTCP packets */
        // FIXME:WORKAROUND: reduce to RTP handle until RTCP is fixed
        n = poll(p, 1, NET_POLL_TIMEOUT);
        if (n > 0) {
// FIXME:WORKAROUND: prevent excessive packet loss
#if 0
            /* first try RTCP */
            if (p[1].revents & POLLIN) {
                from_len = sizeof(from);

                {
                    len = recvfrom(context->rtcpHandle_, buf, buf_size, 0,
                            (struct sockaddr *)&from, &from_len);
                }

                if (len < 0) {
                    if (errno == EAGAIN or errno == EINTR)
                        continue;
                    return -EIO;
                }
                break;
            }
#endif
            /* then RTP */
            if (p[0].revents & POLLIN) {
                from_len = sizeof(from);

                {
                    len = recvfrom(context->rtpHandle_, buf, buf_size, 0,
                            (struct sockaddr *)&from, &from_len);
                }

                if (len < 0) {
                    if (errno == EAGAIN or errno == EINTR)
                        continue;
                    return -EIO;
                }
                break;
            }
        } else if (n < 0) {
            if (errno == EINTR)
                continue;
            return -EIO;
        }
    }
    return len;
}

/* RTCP packet types */
enum RTCPType {
    RTCP_FIR    = 192,
    RTCP_IJ     = 195,
    RTCP_SR     = 200,
    RTCP_TOKEN  = 210
};

#define RTP_PT_IS_RTCP(x) (((x) >= RTCP_FIR && (x) <= RTCP_IJ) || \
                           ((x) >= RTCP_SR  && (x) <= RTCP_TOKEN))

int SocketPair::writeCallback(void *opaque, uint8_t *buf, int buf_size)
{
    SocketPair *context = static_cast<SocketPair*>(opaque);

    int ret;

retry:
    if (RTP_PT_IS_RTCP(buf[1])) {
        /* RTCP payload type */
        std::lock_guard<std::mutex> lock(context->rtcpWriteMutex_);
        ret = ff_network_wait_fd(context->rtcpHandle_);
        if (ret < 0) {
            if (ret == -EAGAIN)
                goto retry;
            return ret;
        }

        ret = sendto(context->rtcpHandle_, buf, buf_size, 0,
                     (sockaddr*) &context->rtcpDestAddr_, context->rtcpDestAddrLen_);
    } else {
        /* RTP payload type */
        ret = ff_network_wait_fd(context->rtpHandle_);
        if (ret < 0) {
            if (ret == -EAGAIN)
                goto retry;
            return ret;
        }

        ret = sendto(context->rtpHandle_, buf, buf_size, 0,
                     (sockaddr*) &context->rtpDestAddr_, context->rtpDestAddrLen_);
    }

    return ret < 0 ? errno : ret;
}

}
