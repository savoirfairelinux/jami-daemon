/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *  Copyright (c) 2007 The FFmpeg Project
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

#include <dhtnet/ip_utils.h> // MUST BE INCLUDED FIRST

#include "libav_deps.h" // THEN THIS ONE AFTER

#include "socket_pair.h"
#include "logger.h"
#include "connectivity/security/memory.h"
#include "media/dtls_srtp.h"

#include <dhtnet/ice_socket.h>

#include <string>
#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>

extern "C" {
#include "srtp.h"
}

#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/types.h>

#ifdef _WIN32
#define SOCK_NONBLOCK FIONBIO
#define poll          WSAPoll
#define close(x)      closesocket(x)
#endif

#ifdef __ANDROID__
#include <asm-generic/fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#ifdef __APPLE__
#include <fcntl.h>
#endif

// Swap 2 byte, 16 bit values:
#define Swap2Bytes(val) ((((val) >> 8) & 0x00FF) | (((val) << 8) & 0xFF00))

// Swap 4 byte, 32 bit values:
#define Swap4Bytes(val) \
    ((((val) >> 24) & 0x000000FF) | (((val) >> 8) & 0x0000FF00) | (((val) << 8) & 0x00FF0000) \
     | (((val) << 24) & 0xFF000000))

// Swap 8 byte, 64 bit values:
#define Swap8Bytes(val) \
    ((((val) >> 56) & 0x00000000000000FF) | (((val) >> 40) & 0x000000000000FF00) \
     | (((val) >> 24) & 0x0000000000FF0000) | (((val) >> 8) & 0x00000000FF000000) \
     | (((val) << 8) & 0x000000FF00000000) | (((val) << 24) & 0x0000FF0000000000) \
     | (((val) << 40) & 0x00FF000000000000) | (((val) << 56) & 0xFF00000000000000))

namespace jami {

static constexpr int NET_POLL_TIMEOUT = 100; /* poll() timeout in ms */
static constexpr int RTP_MAX_PACKET_LENGTH = 2048;
static constexpr auto UDP_HEADER_SIZE = 8;
static constexpr auto SRTP_OVERHEAD = 10;
static constexpr uint32_t RTCP_RR_FRACTION_MASK = 0xFF000000;
static constexpr unsigned MINIMUM_RTP_HEADER_SIZE = 16;
static constexpr unsigned RTP_FIXED_HEADER_SIZE = 12;
static constexpr uint16_t RTP_ONE_BYTE_EXTENSION_PROFILE = 0xBEDE;
static constexpr uint8_t RTCP_PT_SDES = 202;
static constexpr uint8_t RTCP_SDES_END = 0;
static constexpr uint8_t RTCP_SDES_MID_ITEM = 15;

enum class DataType : uint8_t { RTP = 1 << 0, RTCP = 1 << 1 };

static unsigned
getRtpPayloadType(const uint8_t* buf, size_t len)
{
    return len > 1 ? static_cast<unsigned>(buf[1] & 0x7F) : 0u;
}

static uint32_t
readUint32(const uint8_t* data)
{
    return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | uint32_t(data[3]);
}

static void
writeUint32(uint8_t* data, uint32_t value)
{
    data[0] = static_cast<uint8_t>(value >> 24);
    data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(value & 0xFF);
}

struct RtcpMidMapping
{
    uint32_t ssrc {};
    std::string mid {};
};

static std::optional<uint32_t>
getRtcpSenderSsrc(const uint8_t* buf, size_t len)
{
    if (len < 8)
        return std::nullopt;

    const auto packetSize = 4u * (static_cast<size_t>((buf[2] << 8) | buf[3]) + 1u);
    if ((buf[0] >> 6) != 2 || packetSize < 8 || packetSize > len)
        return std::nullopt;

    return readUint32(buf + 4);
}

static std::vector<RtcpMidMapping>
getRtcpMidMappings(const uint8_t* buf, size_t len)
{
    std::vector<RtcpMidMapping> mappings;
    size_t offset = 0;

    while (offset + 4 <= len) {
        const auto version = static_cast<unsigned>(buf[offset] >> 6);
        const auto chunkCount = static_cast<unsigned>(buf[offset] & 0x1F);
        const auto pt = buf[offset + 1];
        const auto packetSize = 4u * (static_cast<size_t>((buf[offset + 2] << 8) | buf[offset + 3]) + 1u);

        if (version != 2 || packetSize < 4 || offset + packetSize > len)
            break;

        if (pt == RTCP_PT_SDES) {
            auto cursor = offset + 4u;
            const auto packetEnd = offset + packetSize;

            for (unsigned chunkIndex = 0; chunkIndex < chunkCount && cursor + 4 <= packetEnd; ++chunkIndex) {
                const auto chunkStart = cursor;
                const auto ssrc = readUint32(buf + cursor);
                cursor += 4u;

                std::string mid;
                while (cursor < packetEnd) {
                    const auto itemType = buf[cursor];
                    if (itemType == RTCP_SDES_END) {
                        ++cursor;
                        while (cursor < packetEnd && ((cursor - chunkStart) % 4u) != 0)
                            ++cursor;
                        break;
                    }

                    if (cursor + 2 > packetEnd) {
                        cursor = packetEnd;
                        break;
                    }

                    const auto itemLen = static_cast<size_t>(buf[cursor + 1]);
                    cursor += 2u;
                    if (cursor + itemLen > packetEnd) {
                        cursor = packetEnd;
                        break;
                    }

                    if (itemType == RTCP_SDES_MID_ITEM)
                        mid.assign(reinterpret_cast<const char*>(buf + cursor), itemLen);

                    cursor += itemLen;
                }

                if (!mid.empty())
                    mappings.emplace_back(RtcpMidMapping {ssrc, std::move(mid)});
            }
        }

        offset += packetSize;
    }

    return mappings;
}

static int
appendRtcpMidSdesItem(const uint8_t* src, int len, uint32_t ssrc, std::string_view mid, uint8_t* dst, size_t dstCapacity)
{
    if (len <= 0 || mid.empty() || mid.size() > std::numeric_limits<uint8_t>::max())
        return len;

    const auto existingMappings = getRtcpMidMappings(src, static_cast<size_t>(len));
    if (std::any_of(existingMappings.begin(), existingMappings.end(), [ssrc, mid](const auto& mapping) {
            return mapping.ssrc == ssrc && mapping.mid == mid;
        })) {
        return len;
    }

    const auto chunkSize = 4u + 2u + mid.size() + 1u;
    const auto paddedChunkSize = (chunkSize + 3u) & ~size_t(3);
    const auto packetSize = 4u + paddedChunkSize;
    const auto newPacketSize = static_cast<size_t>(len) + packetSize;
    if (newPacketSize > dstCapacity)
        return len;

    std::memcpy(dst, src, static_cast<size_t>(len));

    const auto offset = static_cast<size_t>(len);
    dst[offset] = static_cast<uint8_t>(2u << 6) | 1u;
    dst[offset + 1] = RTCP_PT_SDES;

    const auto wordCountMinusOne = static_cast<uint16_t>(packetSize / 4u - 1u);
    dst[offset + 2] = static_cast<uint8_t>(wordCountMinusOne >> 8);
    dst[offset + 3] = static_cast<uint8_t>(wordCountMinusOne & 0xFF);

    writeUint32(dst + offset + 4u, ssrc);
    dst[offset + 8u] = RTCP_SDES_MID_ITEM;
    dst[offset + 9u] = static_cast<uint8_t>(mid.size());
    std::memcpy(dst + offset + 10u, mid.data(), mid.size());
    std::fill(dst + offset + 10u + mid.size(), dst + offset + packetSize, 0);

    return static_cast<int>(newPacketSize);
}

static std::optional<uint32_t>
getRtpSsrc(const uint8_t* buf, size_t len)
{
    if (len < 12)
        return std::nullopt;
    return readUint32(buf + 8);
}

struct RtpHeaderLayout
{
    size_t baseHeaderSize {};
    size_t extensionOffset {};
    size_t extensionPayloadSize {};
    size_t payloadOffset {};
    uint16_t extensionProfile {};
    bool hasExtension {false};
};

static std::optional<RtpHeaderLayout>
getRtpHeaderLayout(const uint8_t* buf, size_t len)
{
    if (len < RTP_FIXED_HEADER_SIZE)
        return std::nullopt;

    RtpHeaderLayout layout;
    layout.baseHeaderSize = RTP_FIXED_HEADER_SIZE + 4u * static_cast<size_t>(buf[0] & 0x0F);
    if (len < layout.baseHeaderSize)
        return std::nullopt;

    layout.hasExtension = (buf[0] & 0x10) != 0;
    layout.payloadOffset = layout.baseHeaderSize;
    if (!layout.hasExtension)
        return layout;

    if (len < layout.baseHeaderSize + 4)
        return std::nullopt;

    layout.extensionOffset = layout.baseHeaderSize;
    layout.extensionProfile = static_cast<uint16_t>((buf[layout.extensionOffset] << 8)
                                                    | buf[layout.extensionOffset + 1]);
    layout.extensionPayloadSize = 4u
                                  * static_cast<size_t>((buf[layout.extensionOffset + 2] << 8)
                                                        | buf[layout.extensionOffset + 3]);
    layout.payloadOffset = layout.extensionOffset + 4 + layout.extensionPayloadSize;
    if (len < layout.payloadOffset)
        return std::nullopt;

    return layout;
}

static std::optional<std::string>
getOneByteRtpExtensionString(const uint8_t* buf, size_t len, unsigned extId)
{
    if (extId == 0 || extId >= 15)
        return std::nullopt;

    const auto layout = getRtpHeaderLayout(buf, len);
    if (!layout || !layout->hasExtension || layout->extensionProfile != RTP_ONE_BYTE_EXTENSION_PROFILE)
        return std::nullopt;

    const auto* extension = buf + layout->extensionOffset + 4;
    size_t remaining = layout->extensionPayloadSize;
    while (remaining > 0) {
        const auto entry = *extension;
        if (entry == 0) {
            ++extension;
            --remaining;
            continue;
        }

        const auto entryId = static_cast<unsigned>(entry >> 4);
        if (entryId == 15)
            break;

        const auto entrySize = static_cast<size_t>((entry & 0x0F) + 1);
        ++extension;
        --remaining;
        if (entrySize > remaining)
            break;

        if (entryId == extId)
            return std::string(reinterpret_cast<const char*>(extension), entrySize);

        extension += entrySize;
        remaining -= entrySize;
    }

    return std::nullopt;
}

static int
appendOneByteRtpExtensionString(
    const uint8_t* src, int len, unsigned extId, std::string_view value, uint8_t* dst, size_t dstCapacity)
{
    if (extId == 0 || extId >= 15 || value.empty() || value.size() > 16 || len <= 0)
        return len;

    const auto layout = getRtpHeaderLayout(src, static_cast<size_t>(len));
    if (!layout)
        return len;

    if (layout->hasExtension) {
        if (layout->extensionProfile != RTP_ONE_BYTE_EXTENSION_PROFILE)
            return len;
        if (getOneByteRtpExtensionString(src, static_cast<size_t>(len), extId))
            return len;
    }

    const auto oldExtensionPayloadSize = layout->hasExtension ? layout->extensionPayloadSize : 0u;
    const auto newEntrySize = 1u + value.size();
    const auto newExtensionPayloadSize = (oldExtensionPayloadSize + newEntrySize + 3u) & ~size_t(3);
    const auto newHeaderSize = layout->baseHeaderSize + 4u + newExtensionPayloadSize;
    const auto newPacketSize = newHeaderSize + (static_cast<size_t>(len) - layout->payloadOffset);

    if (newPacketSize > dstCapacity)
        return len;

    std::memcpy(dst, src, layout->baseHeaderSize);
    dst[0] |= 0x10;

    const auto extensionOffset = layout->baseHeaderSize;
    dst[extensionOffset] = static_cast<uint8_t>(RTP_ONE_BYTE_EXTENSION_PROFILE >> 8);
    dst[extensionOffset + 1] = static_cast<uint8_t>(RTP_ONE_BYTE_EXTENSION_PROFILE & 0xFF);

    const auto extensionWordCount = static_cast<uint16_t>(newExtensionPayloadSize / 4u);
    dst[extensionOffset + 2] = static_cast<uint8_t>(extensionWordCount >> 8);
    dst[extensionOffset + 3] = static_cast<uint8_t>(extensionWordCount & 0xFF);

    auto extensionCursor = extensionOffset + 4;
    if (oldExtensionPayloadSize != 0) {
        std::memcpy(dst + extensionCursor, src + layout->extensionOffset + 4, oldExtensionPayloadSize);
        extensionCursor += oldExtensionPayloadSize;
    }

    dst[extensionCursor++] = static_cast<uint8_t>((extId << 4) | ((value.size() - 1) & 0x0F));
    std::memcpy(dst + extensionCursor, value.data(), value.size());
    extensionCursor += value.size();
    std::fill(dst + extensionCursor, dst + extensionOffset + 4 + newExtensionPayloadSize, 0);

    std::memcpy(dst + newHeaderSize, src + layout->payloadOffset, static_cast<size_t>(len) - layout->payloadOffset);

    return static_cast<int>(newPacketSize);
}

static bool
rtcpReferencesSsrc(const uint8_t* buf, size_t len, uint32_t ssrc)
{
    if (len < 8)
        return false;

    if (readUint32(buf + 4) == ssrc)
        return true;

    const auto pt = buf[1];
    if ((pt == 200 || pt == 201) && len >= 12)
        return readUint32(buf + 8) == ssrc;

    if (pt == 206) {
        if (len >= 12 && readUint32(buf + 8) == ssrc)
            return true;
        if (len >= 24 && readUint32(buf + 20) == ssrc)
            return true;
    }

    return false;
}

class SRTPProtoContext
{
public:
    SRTPProtoContext(const char* out_suite, const char* out_key, const char* in_suite, const char* in_key)
    {
        jami_secure_memzero(&srtp_out, sizeof(srtp_out));
        jami_secure_memzero(&srtp_in, sizeof(srtp_in));
        if (out_suite && out_key) {
            // XXX: see srtp_open from libavformat/srtpproto.c
            if (ff_srtp_set_crypto(&srtp_out, out_suite, out_key) < 0) {
                srtp_close();
                throw std::runtime_error("Unable to set crypto on output");
            }
        }

        if (in_suite && in_key) {
            if (ff_srtp_set_crypto(&srtp_in, in_suite, in_key) < 0) {
                srtp_close();
                throw std::runtime_error("Unable to set crypto on input");
            }
        }
    }

    ~SRTPProtoContext() { srtp_close(); }

    SRTPContext srtp_out {};
    SRTPContext srtp_in {};
    uint8_t encryptbuf[RTP_MAX_PACKET_LENGTH];

private:
    void srtp_close() noexcept
    {
        ff_srtp_free(&srtp_out);
        ff_srtp_free(&srtp_in);
    }
};

struct SocketPair::PacketState
{
    std::mutex dataBuffMutex;
    std::condition_variable cv;
    std::list<std::vector<uint8_t>> rtpDataBuff;
    std::list<std::vector<uint8_t>> rtcpDataBuff;
    std::atomic_bool interrupted {false};
    std::atomic_bool readBlockingMode {false};
    std::optional<unsigned> rtpPayloadType {};
    std::optional<unsigned> rtpMidExtId {};
    std::string remoteMid {};
    std::optional<uint32_t> remoteSsrc {};
    std::optional<uint32_t> localSsrc {};
};

struct SocketPair::BundleContext
{
    BundleContext(std::unique_ptr<dhtnet::IceSocket> rtp_sock,
                  std::unique_ptr<dhtnet::IceSocket> rtcp_sock,
                  bool rtcpMux)
        : rtp_sock_(std::move(rtp_sock))
        , rtcp_sock_(std::move(rtcp_sock))
        , rtcpMux_(rtcpMux)
    {}

    ~BundleContext()
    {
        if (rtp_sock_)
            rtp_sock_->setOnRecv(nullptr);
        if (rtcp_sock_)
            rtcp_sock_->setOnRecv(nullptr);
    }

    void installCallbacks(const std::shared_ptr<BundleContext>& self)
    {
        std::weak_ptr<BundleContext> weakSelf {self};
        if (rtp_sock_) {
            rtp_sock_->setOnRecv([weakSelf](uint8_t* buf, size_t len) {
                if (auto shared = weakSelf.lock())
                    return shared->dispatch(buf, len, false);
                return static_cast<ssize_t>(len);
            });
        }

        if (rtcp_sock_) {
            rtcp_sock_->setOnRecv([weakSelf](uint8_t* buf, size_t len) {
                if (auto shared = weakSelf.lock())
                    return shared->dispatch(buf, len, true);
                return static_cast<ssize_t>(len);
            });
        }
    }

    void registerSubscriber(const std::shared_ptr<PacketState>& subscriber)
    {
        std::lock_guard lk(subscribersMutex_);
        subscribers_.emplace_back(subscriber);
    }

    ssize_t dispatch(uint8_t* buf, size_t len, bool fromRtcpSocket)
    {
        std::vector<std::shared_ptr<PacketState>> subscribers;
        {
            std::lock_guard lk(subscribersMutex_);
            auto it = subscribers_.begin();
            while (it != subscribers_.end()) {
                if (auto subscriber = it->lock()) {
                    subscribers.emplace_back(std::move(subscriber));
                    ++it;
                } else {
                    it = subscribers_.erase(it);
                }
            }
        }

        if (subscribers.empty())
            return static_cast<ssize_t>(len);

        const bool isRtcp = fromRtcpSocket || (rtcpMux_ && SocketPair::isRtcpPacket(buf, len));
        const std::vector<uint8_t> packet(buf, buf + len);

        const auto deliverPacket =
            [&](const std::shared_ptr<PacketState>& subscriber, bool deliverRtcp, std::optional<uint32_t> remoteSsrc) {
                std::lock_guard lk(subscriber->dataBuffMutex);
                if (subscriber->interrupted)
                    return;
                if (remoteSsrc && !subscriber->remoteSsrc)
                    subscriber->remoteSsrc = *remoteSsrc;
                if (deliverRtcp)
                    subscriber->rtcpDataBuff.emplace_back(packet);
                else
                    subscriber->rtpDataBuff.emplace_back(packet);
                subscriber->cv.notify_one();
            };

        if (isRtcp) {
            const auto rtcpMidMappings = getRtcpMidMappings(buf, len);
            if (!rtcpMidMappings.empty()) {
                for (const auto& mapping : rtcpMidMappings) {
                    for (const auto& subscriber : subscribers) {
                        if (subscriber->remoteMid.empty() || subscriber->remoteMid != mapping.mid)
                            continue;

                        std::lock_guard lk(subscriber->dataBuffMutex);
                        subscriber->remoteSsrc = mapping.ssrc;
                    }
                }
            }

            std::vector<std::shared_ptr<PacketState>> matchedSubscribers;
            matchedSubscribers.reserve(subscribers.size());

            for (const auto& subscriber : subscribers) {
                std::optional<uint32_t> remoteSsrc;
                {
                    std::lock_guard lk(subscriber->dataBuffMutex);
                    remoteSsrc = subscriber->remoteSsrc;
                }

                if (!remoteSsrc || rtcpReferencesSsrc(buf, len, *remoteSsrc))
                    matchedSubscribers.emplace_back(subscriber);
            }

            if (matchedSubscribers.empty())
                matchedSubscribers = subscribers;

            for (const auto& subscriber : matchedSubscribers)
                deliverPacket(subscriber, true, std::nullopt);

            return static_cast<ssize_t>(len);
        }

        const auto remoteSsrc = getRtpSsrc(buf, len);
        const auto payloadType = getRtpPayloadType(buf, len);

        std::vector<std::shared_ptr<PacketState>> midMatchedSubscribers;
        bool hasMidRouting = false;
        bool packetHadMid = false;
        for (const auto& subscriber : subscribers) {
            if (!subscriber->rtpMidExtId || subscriber->remoteMid.empty())
                continue;

            hasMidRouting = true;
            if (auto packetMid = getOneByteRtpExtensionString(buf, len, *subscriber->rtpMidExtId); packetMid) {
                packetHadMid = true;
                if (*packetMid == subscriber->remoteMid) {
                    if (subscriber->rtpPayloadType && *subscriber->rtpPayloadType != payloadType)
                        continue;
                    midMatchedSubscribers.emplace_back(subscriber);
                }
            }
        }

        if (!midMatchedSubscribers.empty()) {
            for (const auto& subscriber : midMatchedSubscribers)
                deliverPacket(subscriber, false, remoteSsrc);

            return static_cast<ssize_t>(len);
        }

        if (hasMidRouting && packetHadMid)
            return static_cast<ssize_t>(len);

        for (const auto& subscriber : subscribers) {
            if (subscriber->rtpPayloadType && *subscriber->rtpPayloadType != payloadType)
                continue;
            deliverPacket(subscriber, false, remoteSsrc);
        }

        return static_cast<ssize_t>(len);
    }

    std::unique_ptr<dhtnet::IceSocket> rtp_sock_;
    std::unique_ptr<dhtnet::IceSocket> rtcp_sock_;
    bool rtcpMux_ {false};
    std::mutex subscribersMutex_;
    std::vector<std::weak_ptr<PacketState>> subscribers_;
    // DTLS-SRTP keying material negotiated once on the shared bundle
    // transport. dtlsMutex_ also serializes the handshake itself so a single
    // DTLS association runs per transport.
    std::mutex dtlsMutex_;
    std::optional<DtlsSrtpContext> dtlsSrtpContext_;
};

static int
ff_network_wait_fd(int fd)
{
    struct pollfd p = {fd, POLLOUT, 0};
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
        JAMI_ERROR("socket() failed");
        strErr();
        return -1;
    }

    auto bind_addr = dhtnet::ip_utils::getAnyHostAddr(family);
    if (not bind_addr.isIpv4() and not bind_addr.isIpv6()) {
        JAMI_ERROR("No IPv4/IPv6 host found for family {}", family);
        close(udp_fd);
        return -1;
    }

    bind_addr.setPort(port);
    JAMI_LOG("use local address: {}", bind_addr.toString(true, true));
    if (::bind(udp_fd, bind_addr, bind_addr.getLength()) < 0) {
        JAMI_ERROR("bind() failed");
        strErr();
        close(udp_fd);
        udp_fd = -1;
    }

    return udp_fd;
}

SocketPair::SocketPair(const dhtnet::IpAddr& rtpDestAddr,
                       const dhtnet::IpAddr& rtcpDestAddr,
                       int localRtpPort,
                       int localRtcpPort,
                       bool rtcpMux)
    : packetState_(std::make_shared<PacketState>())
    , rtcpMux_(rtcpMux)
{
    openSockets(rtpDestAddr, rtcpDestAddr, localRtpPort, localRtcpPort);
}

std::shared_ptr<SocketPair::BundleContext>
SocketPair::createBundleContext(std::unique_ptr<dhtnet::IceSocket> rtp_sock,
                                std::unique_ptr<dhtnet::IceSocket> rtcp_sock,
                                bool rtcpMux,
                                bool installReceiveCallbacks)
{
    auto bundleContext = std::make_shared<BundleContext>(std::move(rtp_sock), std::move(rtcp_sock), rtcpMux);

    JAMI_LOG("[{}] Creating shared ICE socket context for comp {} and {} (rtcp-mux={})",
             fmt::ptr(bundleContext.get()),
             bundleContext->rtp_sock_ ? bundleContext->rtp_sock_->getCompId() : -1,
             bundleContext->rtcp_sock_ ? bundleContext->rtcp_sock_->getCompId() : -1,
             rtcpMux ? "yes" : "no");

    if (installReceiveCallbacks)
        bundleContext->installCallbacks(bundleContext);
    return bundleContext;
}

DtlsSrtpContext
SocketPair::ensureBundleDtlsContext(const std::shared_ptr<BundleContext>& bundleContext,
                                    DtlsSetup localSetup,
                                    std::string_view remoteFingerprintType,
                                    std::string_view remoteFingerprint,
                                    const std::shared_ptr<dht::crypto::Certificate>& certificate,
                                    const std::shared_ptr<dht::crypto::PrivateKey>& privateKey,
                                    const std::shared_ptr<std::atomic_bool>& abort)
{
    if (!bundleContext)
        throw std::runtime_error("No bundle context for DTLS-SRTP");

    std::lock_guard lk(bundleContext->dtlsMutex_);
    if (bundleContext->dtlsSrtpContext_)
        return *bundleContext->dtlsSrtpContext_;

    // The handshake owns the shared ICE socket receive callback; the bundle
    // demultiplexer takes over once the handshake completed (RFC 5764
    // requires a single DTLS association per transport).
    auto dtlsContext = negotiateDtlsSrtp(*bundleContext->rtp_sock_,
                                         localSetup,
                                         remoteFingerprintType,
                                         remoteFingerprint,
                                         certificate,
                                         privateKey,
                                         abort);
    bundleContext->installCallbacks(bundleContext);
    bundleContext->dtlsSrtpContext_ = dtlsContext;
    return dtlsContext;
}

SocketPair::SocketPair(std::unique_ptr<dhtnet::IceSocket> rtp_sock,
                       std::unique_ptr<dhtnet::IceSocket> rtcp_sock,
                       bool rtcpMux)
    : packetState_(std::make_shared<PacketState>())
    , rtcpMux_(rtcpMux)
{
    bundleContext_ = createBundleContext(std::move(rtp_sock), std::move(rtcp_sock), rtcpMux_);
    bundleContext_->registerSubscriber(packetState_);
}

SocketPair::SocketPair(const std::shared_ptr<BundleContext>& bundleContext,
                       bool rtcpMux,
                       std::optional<unsigned> rtpPayloadType)
    : packetState_(std::make_shared<PacketState>())
    , bundleContext_(bundleContext)
    , rtcpMux_(rtcpMux)
{
    if (!bundleContext_)
        throw std::runtime_error("Missing shared ICE socket context");

    packetState_->rtpPayloadType = rtpPayloadType;
    bundleContext_->registerSubscriber(packetState_);
}

SocketPair::~SocketPair()
{
    interrupt();
    closeSockets();
    JAMI_LOG("[{}] Instance destroyed", fmt::ptr(this));
}

bool
SocketPair::waitForRTCP(std::chrono::seconds interval)
{
    std::unique_lock lock(rtcpInfo_mutex_);
    return cvRtcpPacketReadyToRead_.wait_for(lock, interval, [this] {
        return packetState_->interrupted or not listRtcpRRHeader_.empty() or not listRtcpREMBHeader_.empty();
    });
}

void
SocketPair::saveRtcpRRPacket(uint8_t* buf, size_t len)
{
    if (len < sizeof(rtcpRRHeader))
        return;

    auto* header = reinterpret_cast<rtcpRRHeader*>(buf);
    if (header->pt != 201) // 201 = RR PT
        return;

    std::lock_guard lock(rtcpInfo_mutex_);

    if (listRtcpRRHeader_.size() >= MAX_LIST_SIZE) {
        listRtcpRRHeader_.pop_front();
    }

    listRtcpRRHeader_.emplace_back(*header);

    cvRtcpPacketReadyToRead_.notify_one();
}

void
SocketPair::saveRtcpREMBPacket(uint8_t* buf, size_t len)
{
    if (len < sizeof(rtcpREMBHeader))
        return;

    auto* header = reinterpret_cast<rtcpREMBHeader*>(buf);
    if (header->pt != 206) // 206 = REMB PT
        return;

    if (header->uid != 0x424D4552) // uid must be "REMB"
        return;

    std::lock_guard lock(rtcpInfo_mutex_);

    if (listRtcpREMBHeader_.size() >= MAX_LIST_SIZE) {
        listRtcpREMBHeader_.pop_front();
    }

    listRtcpREMBHeader_.push_back(*header);

    cvRtcpPacketReadyToRead_.notify_one();
}

std::list<rtcpRRHeader>
SocketPair::getRtcpRR()
{
    std::lock_guard lock(rtcpInfo_mutex_);
    return std::move(listRtcpRRHeader_);
}

std::list<rtcpREMBHeader>
SocketPair::getRtcpREMB()
{
    std::lock_guard lock(rtcpInfo_mutex_);
    return std::move(listRtcpREMBHeader_);
}

void
SocketPair::createSRTP(const char* out_suite, const char* out_key, const char* in_suite, const char* in_key)
{
    srtpContext_.reset(new SRTPProtoContext(out_suite, out_key, in_suite, in_key));
}

void
SocketPair::queuePacket(std::vector<uint8_t>&& packet, bool isRtcp)
{
    std::lock_guard l(packetState_->dataBuffMutex);
    if (packetState_->interrupted)
        return;
    if (isRtcp)
        packetState_->rtcpDataBuff.emplace_back(std::move(packet));
    else
        packetState_->rtpDataBuff.emplace_back(std::move(packet));
    packetState_->cv.notify_one();
}

void
SocketPair::setLocalSsrc(uint32_t ssrc)
{
    if (ssrc == 0)
        return;

    std::lock_guard l(packetState_->dataBuffMutex);
    packetState_->localSsrc = ssrc;
}

void
SocketPair::setRemoteSsrc(uint32_t ssrc)
{
    if (ssrc == 0)
        return;

    std::lock_guard l(packetState_->dataBuffMutex);
    packetState_->remoteSsrc = ssrc;
}

std::optional<uint32_t>
SocketPair::getLocalSsrc() const
{
    std::lock_guard l(packetState_->dataBuffMutex);
    return packetState_->localSsrc;
}

std::optional<uint32_t>
SocketPair::getRemoteSsrc() const
{
    std::lock_guard l(packetState_->dataBuffMutex);
    return packetState_->remoteSsrc;
}

std::vector<uint8_t>
SocketPair::createRtcpPli(uint32_t senderSsrc, uint32_t mediaSsrc)
{
    // RFC 4585 6.3.1: PSFB (PT 206) with FMT 1 and no FCI.
    std::vector<uint8_t> packet;
    packet.reserve(12);
    packet.push_back(0x80 | 1); // V=2, P=0, FMT=1 (PLI)
    packet.push_back(206);      // PT=PSFB
    packet.push_back(0);
    packet.push_back(2); // length = 2 32-bit words minus one
    for (int shift = 24; shift >= 0; shift -= 8)
        packet.push_back(static_cast<uint8_t>(senderSsrc >> shift));
    for (int shift = 24; shift >= 0; shift -= 8)
        packet.push_back(static_cast<uint8_t>(mediaSsrc >> shift));
    return packet;
}

bool
SocketPair::isRtcpKeyframeRequest(const uint8_t* buf, size_t len)
{
    if (!buf || len < 12)
        return false;
    if ((buf[0] >> 6) != 2 || buf[1] != 206)
        return false;

    const auto fmt = buf[0] & 0x1f;
    // PLI (RFC 4585 6.3.1) or FIR (RFC 5104 4.3.1)
    return fmt == 1 || fmt == 4;
}

bool
SocketPair::isRtcpPacket(const uint8_t* buf, size_t len)
{
    return len > 1 && RTP_PT_IS_RTCP(buf[1]);
}

void
SocketPair::interrupt()
{
    JAMI_WARNING("[{}] Interrupting RTP sockets", fmt::ptr(this));
    packetState_->interrupted = true;
    packetState_->cv.notify_all();
    cvRtcpPacketReadyToRead_.notify_all();
}

void
SocketPair::setReadBlockingMode(bool block)
{
    JAMI_LOG("[{}] Read operations in blocking mode [{}]", fmt::ptr(this), block ? "YES" : "NO");
    packetState_->readBlockingMode = block;
    packetState_->cv.notify_all();
    cvRtcpPacketReadyToRead_.notify_all();
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
SocketPair::setDefaultRemoteAddresses(const dhtnet::IpAddr& rtpDestAddr, const dhtnet::IpAddr& rtcpDestAddr)
{
    rtpDestAddr_ = rtpDestAddr;
    rtcpDestAddr_ = rtcpDestAddr;

    if (auto* rtpSock = getRtpSocket(); rtpSock && rtpDestAddr)
        rtpSock->setDefaultRemoteAddress(rtpDestAddr);
    if (auto* rtcpSock = getRtcpSocket(); rtcpSock && rtcpDestAddr)
        rtcpSock->setDefaultRemoteAddress(rtcpDestAddr);
}

dhtnet::IceSocket*
SocketPair::getRtpSocket() const
{
    return bundleContext_ ? bundleContext_->rtp_sock_.get() : nullptr;
}

dhtnet::IceSocket*
SocketPair::getRtcpSocket() const
{
    return bundleContext_ ? bundleContext_->rtcp_sock_.get() : nullptr;
}

void
SocketPair::openSockets(const dhtnet::IpAddr& rtpDestAddr,
                        const dhtnet::IpAddr& rtcpDestAddr,
                        int local_rtp_port,
                        int local_rtcp_port)
{
    rtpDestAddr_ = rtpDestAddr;
    rtcpDestAddr_ = rtcpDestAddr;

    JAMI_LOG("Creating RTP sockets for remote{{{},{}}} local{{{},{}}} (rtcp-mux={})",
             rtpDestAddr_.toString(true),
             rtcpDestAddr_.toString(true),
             local_rtp_port,
             local_rtcp_port,
             rtcpMux_ ? "yes" : "no");

    // Open local sockets (RTP/RTCP)
    if ((rtpHandle_ = udp_socket_create(rtpDestAddr_.getFamily(), local_rtp_port)) == -1
        or (!rtcpMux_ && (rtcpHandle_ = udp_socket_create(rtcpDestAddr_.getFamily(), local_rtcp_port)) == -1)) {
        closeSockets();
        JAMI_ERROR("[{}] Sockets creation failed", fmt::ptr(this));
        throw std::runtime_error("Sockets creation failed");
    }

    JAMI_LOG("SocketPair: local{{{},{}}} / {}{{{},{}}}",
             local_rtp_port,
             rtcpMux_ ? local_rtp_port : local_rtcp_port,
             rtpDestAddr_.toString(false),
             rtpDestAddr_.getPort(),
             rtcpMux_ ? rtpDestAddr_.getPort() : rtcpDestAddr_.getPort());
}

MediaIOHandle*
SocketPair::createIOContext(const uint16_t mtu)
{
    unsigned ip_header_size;
    if (auto* rtpSock = getRtpSocket())
        ip_header_size = rtpSock->getTransportOverhead();
    else if (rtpDestAddr_.getFamily() == AF_INET6)
        ip_header_size = 40;
    else
        ip_header_size = 20;
    return new MediaIOHandle(
        mtu - (srtpContext_ ? SRTP_OVERHEAD : 0) - UDP_HEADER_SIZE - ip_header_size,
        true,
        [](void* sp, uint8_t* buf, int len) { return static_cast<SocketPair*>(sp)->readCallback(buf, len); },
        [](void* sp, io_writebuffer* buf, int len) { return static_cast<SocketPair*>(sp)->writeCallback(buf, len); },
        0,
        reinterpret_cast<void*>(this));
}

int
SocketPair::waitForData()
{
    // System sockets
    if (rtpHandle_ >= 0) {
        int ret;
        do {
            if (packetState_->interrupted) {
                errno = EINTR;
                return -1;
            }

            if (not packetState_->readBlockingMode) {
                return 0;
            }

            if (rtcpMux_) {
                struct pollfd p[1] = {{rtpHandle_, POLLIN, 0}};
                ret = poll(p, 1, NET_POLL_TIMEOUT);
                if (ret > 0 && (p[0].revents & POLLIN))
                    ret = static_cast<int>(DataType::RTP) | static_cast<int>(DataType::RTCP);
            } else {
                // work with system socket
                struct pollfd p[2] = {{rtpHandle_, POLLIN, 0}, {rtcpHandle_, POLLIN, 0}};
                ret = poll(p, 2, NET_POLL_TIMEOUT);
                if (ret > 0) {
                    ret = 0;
                    if (p[0].revents & POLLIN)
                        ret |= static_cast<int>(DataType::RTP);
                    if (p[1].revents & POLLIN)
                        ret |= static_cast<int>(DataType::RTCP);
                }
            }
        } while (!ret or (ret < 0 and errno == EAGAIN));

        return ret;
    }

    // work with IceSocket
    {
        std::unique_lock lk(packetState_->dataBuffMutex);
        packetState_->cv.wait(lk, [this] {
            return packetState_->interrupted or not packetState_->rtpDataBuff.empty()
                   or not packetState_->rtcpDataBuff.empty() or not packetState_->readBlockingMode;
        });
    }

    if (packetState_->interrupted) {
        errno = EINTR;
        return -1;
    }

    return static_cast<int>(DataType::RTP) | static_cast<int>(DataType::RTCP);
}

int
SocketPair::queueMuxedSocketData()
{
    std::array<uint8_t, RTP_MAX_PACKET_LENGTH> packet;
    struct sockaddr_storage from;
    socklen_t from_len = sizeof(from);
    auto received = recvfrom(rtpHandle_,
                             reinterpret_cast<char*>(packet.data()),
                             packet.size(),
                             0,
                             reinterpret_cast<struct sockaddr*>(&from),
                             &from_len);
    if (received <= 0)
        return static_cast<int>(received);

    const auto len = static_cast<size_t>(received);
    queuePacket(std::vector<uint8_t>(packet.begin(), packet.begin() + len), isRtcpPacket(packet.data(), len));
    return static_cast<int>(len);
}

int
SocketPair::readRtpData(void* buf, int buf_size)
{
    if (rtcpMux_ && rtpHandle_ >= 0) {
        std::unique_lock lk(packetState_->dataBuffMutex);
        if (not packetState_->rtpDataBuff.empty()) {
            auto pkt = std::move(packetState_->rtpDataBuff.front());
            packetState_->rtpDataBuff.pop_front();
            lk.unlock();
            int pkt_size = static_cast<int>(pkt.size());
            int len = std::min(pkt_size, buf_size);
            std::copy_n(pkt.begin(), len, static_cast<char*>(buf));
            return len;
        }

        return 0;
    }

    // handle system socket
    if (rtpHandle_ >= 0) {
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);
        return static_cast<int>(recvfrom(rtpHandle_,
                                         static_cast<char*>(buf),
                                         buf_size,
                                         0,
                                         reinterpret_cast<struct sockaddr*>(&from),
                                         &from_len));
    }

    // handle ICE
    std::unique_lock lk(packetState_->dataBuffMutex);
    if (not packetState_->rtpDataBuff.empty()) {
        auto pkt = std::move(packetState_->rtpDataBuff.front());
        packetState_->rtpDataBuff.pop_front();
        lk.unlock(); // to not block our ICE callbacks
        int pkt_size = static_cast<int>(pkt.size());
        int len = std::min(pkt_size, buf_size);
        std::copy_n(pkt.begin(), len, static_cast<char*>(buf));
        return len;
    }

    return 0;
}

int
SocketPair::readRtcpData(void* buf, int buf_size)
{
    if (rtcpMux_ && rtpHandle_ >= 0) {
        std::unique_lock lk(packetState_->dataBuffMutex);
        if (not packetState_->rtcpDataBuff.empty()) {
            auto pkt = std::move(packetState_->rtcpDataBuff.front());
            packetState_->rtcpDataBuff.pop_front();
            lk.unlock();
            int pkt_size = static_cast<int>(pkt.size());
            int len = std::min(pkt_size, buf_size);
            std::copy_n(pkt.begin(), len, static_cast<char*>(buf));
            return len;
        }

        return 0;
    }

    // handle system socket
    if (rtcpHandle_ >= 0) {
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);
        return static_cast<int>(recvfrom(rtcpHandle_,
                                         static_cast<char*>(buf),
                                         buf_size,
                                         0,
                                         reinterpret_cast<struct sockaddr*>(&from),
                                         &from_len));
    }

    // handle ICE
    std::unique_lock lk(packetState_->dataBuffMutex);
    if (not packetState_->rtcpDataBuff.empty()) {
        auto pkt = std::move(packetState_->rtcpDataBuff.front());
        packetState_->rtcpDataBuff.pop_front();
        lk.unlock();
        int pkt_size = static_cast<int>(pkt.size());
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

    if (rtcpMux_ && rtpHandle_ >= 0 && packetState_->rtpDataBuff.empty() && packetState_->rtcpDataBuff.empty()) {
        auto queued = queueMuxedSocketData();
        if (queued < 0)
            return queued;
    }

    int len = 0;
    bool fromRTCP = false;

    if (datatype & static_cast<int>(DataType::RTCP)) {
        len = readRtcpData(buf, buf_size);
        // SRTCP decrypt (RFC 3711 3.4). Fall back to the packet as-is on
        // failure: legacy peers exchange plaintext RTCP.
        if (len > 0 and rtcpProtection_ and srtpContext_ and srtpContext_->srtp_in.aes) {
            int decryptedLen = len;
            if (ff_srtp_decrypt(&srtpContext_->srtp_in, buf, &decryptedLen) == 0)
                len = decryptedLen;
        }
        if (len > 0) {
            size_t offset = 0;
            while (offset + 4 <= static_cast<size_t>(len)) {
                auto* packet = buf + offset;
                const auto packetSize = 4u * (static_cast<size_t>((packet[2] << 8) | packet[3]) + 1u);
                if ((packet[0] >> 6) != 2 || packetSize < 4 || offset + packetSize > static_cast<size_t>(len))
                    break;

                auto* header = reinterpret_cast<rtcpRRHeader*>(packet);
                // 201 = RR PT
                if (header->pt == 201) {
                    lastDLSR_ = Swap4Bytes(header->dlsr);
                    // JAMI_WARN("Read RR, lastDLSR : %d", lastDLSR_);
                    lastRR_time = std::chrono::steady_clock::now();
                    saveRtcpRRPacket(packet, packetSize);
                }
                // 206 = PSFB PT: REMB (FMT 15), PLI (FMT 1), FIR (FMT 4)
                else if (header->pt == 206) {
                    if (isRtcpKeyframeRequest(packet, packetSize)) {
                        if (keyframeRequestCallback_)
                            keyframeRequestCallback_();
                    } else {
                        saveRtcpREMBPacket(packet, packetSize);
                    }
                }
                // 200 = SR PT
                else if (header->pt == 200) {
                    // not used yet
                } else {
                    unsigned pt = header->pt;
                    JAMI_LOG("Unable to read RTCP: unknown packet type {}", pt);
                }

                offset += packetSize;
            }
            fromRTCP = true;
        }
    }

    // No RTCP… attempt RTP
    if (!len and (datatype & static_cast<int>(DataType::RTP))) {
        len = readRtpData(buf, buf_size);
        fromRTCP = false;
    }

    if (len <= 0)
        return len;

    if (not fromRTCP && (buf_size < static_cast<int>(MINIMUM_RTP_HEADER_SIZE)))
        return len;

    // SRTP decrypt
    if (not fromRTCP and srtpContext_ and srtpContext_->srtp_in.aes) {
        int32_t gradient = 0;
        int32_t deltaT = 0;
        float abs = 0.0f;
        bool res_parse = false;
        bool res_delay = false;

        res_parse = parse_RTP_ext(buf, &abs);
        bool marker = (buf[1] & 0x80) >> 7;

        if (res_parse)
            res_delay = getOneWayDelayGradient(abs, marker, &gradient, &deltaT);

        // rtpDelayCallback_ is not set for audio
        if (rtpDelayCallback_ and res_delay)
            rtpDelayCallback_(gradient, deltaT);

        auto err = ff_srtp_decrypt(&srtpContext_->srtp_in, buf, &len);
        if (packetLossCallback_ and (buf[2] << 8 | buf[3]) != lastSeqNumIn_ + 1)
            packetLossCallback_();
        lastSeqNumIn_ = buf[2] << 8 | buf[3];
        if (err < 0)
            JAMI_WARNING("decrypt error {}", err);
    }

    if (len != 0)
        return len;
    else
        return AVERROR_EOF;
}

int
SocketPair::writeData(const uint8_t* buf, int buf_size)
{
    bool isRTCP = RTP_PT_IS_RTCP(buf[1]);

    // System sockets?
    if (rtpHandle_ >= 0) {
        int fd;
        dhtnet::IpAddr* dest_addr;

        if (rtcpMux_) {
            fd = rtpHandle_;
            dest_addr = &rtpDestAddr_;
        } else if (isRTCP) {
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
        return static_cast<int>(
            ::sendto(fd, reinterpret_cast<const char*>(buf), buf_size, 0, *dest_addr, dest_addr->getLength()));
    }

    if (noWrite_)
        return buf_size;

    // IceSocket
    auto* rtpSock = getRtpSocket();
    if (!rtpSock)
        return -EIO;

    if (rtcpMux_ || !isRTCP)
        return static_cast<int>(rtpSock->send(buf, buf_size));

    if (auto* rtcpSock = getRtcpSocket())
        return static_cast<int>(rtcpSock->send(buf, buf_size));

    return static_cast<int>(rtpSock->send(buf, buf_size));
}

int
SocketPair::writeRtcpData(const uint8_t* buf, int buf_size)
{
    if (!buf || buf_size <= 1 || !RTP_PT_IS_RTCP(buf[1]))
        return -EINVAL;

    return writeCallback(buf, buf_size);
}

int
SocketPair::writeCallback(const uint8_t* buf, int buf_size)
{
    if (noWrite_)
        return 0;

    int ret;
    bool isRTCP = RTP_PT_IS_RTCP(buf[1]);
    std::array<uint8_t, RTP_MAX_PACKET_LENGTH> bundlePacket {};
    if (!isRTCP) {
        if (const auto ssrc = getRtpSsrc(buf, static_cast<size_t>(buf_size)))
            setLocalSsrc(*ssrc);
    }
    unsigned int ts_LSB, ts_MSB;
    double currentSRTS, currentLatency;

    if (!isRTCP && localRtpMidExtId_ && !localRtpMid_.empty()) {
        const auto patchedSize = appendOneByteRtpExtensionString(buf,
                                                                 buf_size,
                                                                 *localRtpMidExtId_,
                                                                 localRtpMid_,
                                                                 bundlePacket.data(),
                                                                 bundlePacket.size());
        if (patchedSize != buf_size) {
            buf = bundlePacket.data();
            buf_size = patchedSize;
        }
    } else if (isRTCP && !localRtpMid_.empty()) {
        if (const auto senderSsrc = getRtcpSenderSsrc(buf, static_cast<size_t>(buf_size)); senderSsrc) {
            const auto patchedSize = appendRtcpMidSdesItem(buf,
                                                           buf_size,
                                                           *senderSsrc,
                                                           localRtpMid_,
                                                           bundlePacket.data(),
                                                           bundlePacket.size());
            if (patchedSize != buf_size) {
                buf = bundlePacket.data();
                buf_size = patchedSize;
            }
        }
    }

    // Inspect outgoing RTCP while it is still plaintext.
    // Check if we're sending an RR, if so, detect packet loss.
    // buf_size gives length of buffer, not just header.
    if (isRTCP && static_cast<unsigned>(buf_size) >= sizeof(rtcpRRHeader)) {
        auto* header = reinterpret_cast<const rtcpRRHeader*>(buf);
        rtcpPacketLoss_ = (header->pt == 201 && ntohl(header->fraction_lost) & RTCP_RR_FRACTION_MASK);
    }
    if (isRTCP && buf[1] == 200 && static_cast<unsigned>(buf_size) >= sizeof(rtcpSRHeader)) // Sender Report
    {
        auto* header = reinterpret_cast<const rtcpSRHeader*>(buf);
        ts_LSB = Swap4Bytes(header->timestampLSB);
        ts_MSB = Swap4Bytes(header->timestampMSB);

        currentSRTS = ts_MSB + (ts_LSB / pow(2, 32));

        if (lastSRTS_ != 0 && lastDLSR_ != 0) {
            if (histoLatency_.size() >= MAX_LIST_SIZE)
                histoLatency_.pop_front();

            currentLatency = (currentSRTS - lastSRTS_) / 2;
            histoLatency_.push_back(currentLatency);
        }

        lastSRTS_ = currentSRTS;
    }

    // Encrypt? RTCP is only protected when SRTCP was negotiated (DTLS-SRTP),
    // legacy SDES peers expect plaintext RTCP.
    if ((not isRTCP or rtcpProtection_) and srtpContext_ and srtpContext_->srtp_out.aes) {
        buf_size = ff_srtp_encrypt(&srtpContext_->srtp_out,
                                   buf,
                                   buf_size,
                                   srtpContext_->encryptbuf,
                                   sizeof(srtpContext_->encryptbuf));
        if (buf_size < 0) {
            JAMI_WARNING("encrypt error {}", buf_size);
            return buf_size;
        }

        buf = srtpContext_->encryptbuf;
    }

    do {
        if (packetState_->interrupted)
            return -EINTR;
        ret = writeData(buf, buf_size);
    } while (ret < 0 and errno == EAGAIN);

    return ret < 0 ? -errno : ret;
}

double
SocketPair::getLastLatency()
{
    if (not histoLatency_.empty())
        return histoLatency_.back();
    else
        return -1;
}

void
SocketPair::setRtpDelayCallback(std::function<void(int, int)> cb)
{
    rtpDelayCallback_ = std::move(cb);
}

void
SocketPair::setBundleMidExtension(std::string localMid,
                                  std::optional<unsigned> localMidExtId,
                                  std::string remoteMid,
                                  std::optional<unsigned> remoteMidExtId)
{
    localRtpMid_ = std::move(localMid);
    localRtpMidExtId_ = (localMidExtId && *localMidExtId != 0) ? localMidExtId : std::nullopt;
    if (localRtpMid_.size() > 16) {
        JAMI_WARNING("Ignoring MID RTP extension for oversized local MID {}", localRtpMid_);
        localRtpMidExtId_.reset();
    }

    std::lock_guard lk(packetState_->dataBuffMutex);
    packetState_->remoteMid = std::move(remoteMid);
    packetState_->rtpMidExtId = (remoteMidExtId && *remoteMidExtId != 0) ? remoteMidExtId : std::nullopt;
}

bool
SocketPair::getOneWayDelayGradient(float sendTS, bool marker, int32_t* gradient, int32_t* deltaT)
{
    // Keep only last packet of each frame
    if (not marker) {
        return false;
    }

    // 1st frame
    if (lastSendTS_ == 0.0f) {
        lastSendTS_ = sendTS;
        lastReceiveTS_ = std::chrono::steady_clock::now();
        return false;
    }

    int32_t deltaS = static_cast<int32_t>((sendTS - lastSendTS_) * 1000); // milliseconds
    if (deltaS < 0)
        deltaS += 64000;
    lastSendTS_ = sendTS;

    std::chrono::steady_clock::time_point arrival_TS = std::chrono::steady_clock::now();
    auto deltaR = static_cast<int32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(arrival_TS - lastReceiveTS_).count());
    lastReceiveTS_ = arrival_TS;

    *gradient = deltaR - deltaS;
    *deltaT = deltaR;

    return true;
}

bool
SocketPair::parse_RTP_ext(uint8_t* buf, float* abs)
{
    if (not(buf[0] & 0x10))
        return false;

    uint16_t magic_word = (buf[12] << 8) + buf[13];
    if (magic_word != 0xBEDE)
        return false;

    uint8_t sec = buf[17] >> 2;
    uint32_t fract = ((buf[17] & 0x3) << 16 | (buf[18] << 8) | buf[19]) << 14;
    float milli = static_cast<float>(fract / pow(2, 32));

    *abs = static_cast<float>(sec) + (milli);
    return true;
}

uint16_t
SocketPair::lastSeqValOut()
{
    if (srtpContext_)
        return srtpContext_->srtp_out.seq_largest;
    JAMI_ERROR("SRTP context not found.");
    return 0;
}

} // namespace jami
