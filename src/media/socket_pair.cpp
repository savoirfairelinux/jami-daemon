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
#include "media/transport_cc.h"

#include <dhtnet/ice_socket.h>

#include <string>
#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>
#include <thread>
#include <unordered_set>

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
static constexpr int64_t RTP_PACING_MAX_QUEUE_DELAY_US = 200'000;
static constexpr uint32_t RTCP_RR_FRACTION_MASK = 0xFF000000;
static constexpr unsigned MINIMUM_RTP_HEADER_SIZE = 16;
static constexpr unsigned RTP_FIXED_HEADER_SIZE = 12;
static constexpr uint16_t RTP_ONE_BYTE_EXTENSION_PROFILE = 0xBEDE;
static constexpr uint8_t RTCP_PT_SDES = 202;
static constexpr uint8_t RTCP_PT_BYE = 203;
static constexpr uint8_t RTCP_PT_RTPFB = 205;
static constexpr uint8_t RTCP_SDES_END = 0;
static constexpr uint8_t RTCP_SDES_MID_ITEM = 15;
static constexpr uint8_t RTCP_RTPFB_GENERIC_NACK = 1;
static constexpr size_t MAX_CACHED_RTP_PACKETS = 8192;
static constexpr auto MAX_CACHED_RTP_PACKET_AGE = std::chrono::seconds(10);
static constexpr size_t MAX_NACK_RETRANSMISSIONS = 64;

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
stripRtcpByePackets(uint8_t* buf, int len)
{
    size_t readOffset = 0;
    size_t writeOffset = 0;

    while (readOffset + 4 <= static_cast<size_t>(len)) {
        const auto* packet = buf + readOffset;
        const auto packetSize = 4u * (static_cast<size_t>((packet[2] << 8) | packet[3]) + 1u);
        if ((packet[0] >> 6) != 2 || packetSize < 4 || readOffset + packetSize > static_cast<size_t>(len))
            return len;

        if (packet[1] != RTCP_PT_BYE) {
            if (writeOffset != readOffset)
                std::memmove(buf + writeOffset, packet, packetSize);
            writeOffset += packetSize;
        }
        readOffset += packetSize;
    }

    return readOffset == static_cast<size_t>(len) ? static_cast<int>(writeOffset) : len;
}

static bool
isCompleteRtcpCompoundPacket(const uint8_t* buf, int len)
{
    size_t offset = 0;

    while (offset + 4 <= static_cast<size_t>(len)) {
        const auto* packet = buf + offset;
        const auto packetSize = 4u * (static_cast<size_t>((packet[2] << 8) | packet[3]) + 1u);
        if ((packet[0] >> 6) != 2 || !RTP_PT_IS_RTCP(packet[1]) || packetSize < 4
            || offset + packetSize > static_cast<size_t>(len))
            return false;
        offset += packetSize;
    }

    return offset == static_cast<size_t>(len);
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

static size_t
getOneByteRtpExtensionContentSize(const uint8_t* extension, size_t payloadSize)
{
    size_t offset = 0;
    size_t contentSize = 0;
    while (offset < payloadSize) {
        const auto entry = extension[offset];
        if (entry == 0) {
            ++offset;
            continue;
        }

        const auto entryId = static_cast<unsigned>(entry >> 4);
        if (entryId == 15)
            break;

        const auto entrySize = static_cast<size_t>((entry & 0x0F) + 1);
        const auto entryEnd = offset + 1 + entrySize;
        if (entryEnd > payloadSize)
            break;

        offset = entryEnd;
        contentSize = entryEnd;
    }
    return contentSize;
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
    const auto oldExtensionContentSize = layout->hasExtension
                                             ? getOneByteRtpExtensionContentSize(src + layout->extensionOffset + 4,
                                                                                 oldExtensionPayloadSize)
                                             : 0u;
    const auto newEntrySize = 1u + value.size();
    const auto newExtensionPayloadSize = (oldExtensionContentSize + newEntrySize + 3u) & ~size_t(3);
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
    if (oldExtensionContentSize != 0) {
        std::memcpy(dst + extensionCursor, src + layout->extensionOffset + 4, oldExtensionContentSize);
        extensionCursor += oldExtensionContentSize;
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

    if (pt == RTCP_PT_RTPFB && len >= 12)
        return readUint32(buf + 8) == ssrc;

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
    std::optional<unsigned> transportCcExtId {};
    std::string remoteMid {};
    std::optional<uint32_t> remoteSsrc {};
    std::optional<uint32_t> localSsrc {};
    std::shared_ptr<TransportCcState> transportCcState {};
};

struct SocketPair::TransportCcState
{
    struct ReceivedPacket
    {
        uint16_t sequenceNumber {};
        std::chrono::steady_clock::time_point receiveTime {};
    };

    struct SentPacket
    {
        uint16_t sequenceNumber {};
        std::chrono::steady_clock::time_point sendTime {};
        size_t packetSize {};
    };

    std::mutex mutex;
    uint16_t nextSequenceNumber {};
    uint8_t feedbackPacketCount {};
    std::vector<ReceivedPacket> receivedPackets {};
    std::vector<SentPacket> sentPackets {};
};

struct SocketPair::BundleContext
{
    BundleContext(std::unique_ptr<dhtnet::IceSocket> rtp_sock,
                  std::unique_ptr<dhtnet::IceSocket> rtcp_sock,
                  bool rtcpMux)
        : rtp_sock_(std::move(rtp_sock))
        , rtcp_sock_(std::move(rtcp_sock))
        , rtcpMux_(rtcpMux)
        , transportCcState_(std::make_shared<TransportCcState>())
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
                if (!deliverRtcp)
                    SocketPair::recordTransportCcReceive(subscriber, packet.data(), packet.size());

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

            std::vector<std::shared_ptr<PacketState>> localMatches;
            std::vector<std::shared_ptr<PacketState>> remoteMatches;
            localMatches.reserve(subscribers.size());
            remoteMatches.reserve(subscribers.size());

            for (const auto& subscriber : subscribers) {
                std::optional<uint32_t> localSsrc;
                std::optional<uint32_t> remoteSsrc;
                {
                    std::lock_guard lk(subscriber->dataBuffMutex);
                    localSsrc = subscriber->localSsrc;
                    remoteSsrc = subscriber->remoteSsrc;
                }

                if (localSsrc && rtcpReferencesSsrc(buf, len, *localSsrc))
                    localMatches.emplace_back(subscriber);
                if (remoteSsrc && rtcpReferencesSsrc(buf, len, *remoteSsrc))
                    remoteMatches.emplace_back(subscriber);
            }

            const auto& matchedSubscribers = !localMatches.empty()
                                                 ? localMatches
                                                 : (!remoteMatches.empty() ? remoteMatches : subscribers);

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
    std::shared_ptr<TransportCcState> transportCcState_;
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
    packetState_->transportCcState = std::make_shared<TransportCcState>();
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

void
SocketPair::setBundleDtlsContext(const std::shared_ptr<BundleContext>& bundleContext,
                                 const DtlsSrtpContext& context)
{
    if (!bundleContext)
        throw std::runtime_error("No bundle context for DTLS-SRTP");

    std::lock_guard lk(bundleContext->dtlsMutex_);
    if (bundleContext->dtlsSrtpContext_)
        return;

    bundleContext->dtlsSrtpContext_ = context;
    bundleContext->installCallbacks(bundleContext);
}

SocketPair::SocketPair(std::unique_ptr<dhtnet::IceSocket> rtp_sock,
                       std::unique_ptr<dhtnet::IceSocket> rtcp_sock,
                       bool rtcpMux)
    : packetState_(std::make_shared<PacketState>())
    , rtcpMux_(rtcpMux)
{
    packetState_->transportCcState = std::make_shared<TransportCcState>();
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
    packetState_->transportCcState = bundleContext_->transportCcState_;
    bundleContext_->registerSubscriber(packetState_);
}

SocketPair::~SocketPair()
{
    interrupt();
    closeSockets();
    JAMI_LOG("[{}] Instance destroyed", fmt::ptr(this));
}

bool
SocketPair::waitForRTCP(std::chrono::milliseconds interval)
{
    std::unique_lock lock(rtcpInfo_mutex_);
    return cvRtcpPacketReadyToRead_.wait_for(lock, interval, [this] {
        return packetState_->interrupted or not listRtcpRRHeader_.empty() or not listRtcpREMBHeader_.empty()
               or not listRtcpTransportCc_.empty();
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

void
SocketPair::saveRtcpTransportCcPacket(uint8_t* buf, size_t len)
{
    auto feedback = parseTransportCcFeedbackPacket(buf, len);
    if (!feedback)
        return;

    auto report = createTransportCcReport(*feedback);

    std::lock_guard lock(rtcpInfo_mutex_);

    if (listRtcpTransportCc_.size() >= MAX_LIST_SIZE)
        listRtcpTransportCc_.pop_front();

    listRtcpTransportCc_.push_back(std::move(*feedback));

    if (report) {
        if (listRtcpTransportCcReports_.size() >= MAX_LIST_SIZE)
            listRtcpTransportCcReports_.pop_front();
        listRtcpTransportCcReports_.push_back(std::move(*report));
    }

    cvRtcpPacketReadyToRead_.notify_one();
}

void
SocketPair::cacheSentRtpPacket(const uint8_t* buf, size_t len)
{
    if (!buf || len < RTP_FIXED_HEADER_SIZE)
        return;

    const auto now = clock::now();
    std::lock_guard lock(sentRtpPacketsMutex_);
    while (!sentRtpPackets_.empty()
           && (sentRtpPackets_.size() >= MAX_CACHED_RTP_PACKETS
               || now - sentRtpPackets_.front().sentAt > MAX_CACHED_RTP_PACKET_AGE)) {
        sentRtpPackets_.pop_front();
    }
    const auto sequence = static_cast<uint16_t>(buf[2] << 8 | buf[3]);
    const auto ssrc = readUint32(buf + 8);
    sentRtpPackets_.push_back({
        sequence,
        ssrc,
        now,
        std::vector<uint8_t>(buf, buf + len),
    });
}

void
SocketPair::retransmitNackPackets(const uint8_t* buf, size_t len)
{
    if (!buf || len < 16 || buf[1] != RTCP_PT_RTPFB
        || (buf[0] & 0x1f) != RTCP_RTPFB_GENERIC_NACK) {
        return;
    }

    auto feedbackEnd = len;
    if ((buf[0] & 0x20) != 0) {
        const auto paddingSize = static_cast<size_t>(buf[len - 1]);
        if (paddingSize == 0 || paddingSize > len - 12)
            return;
        feedbackEnd -= paddingSize;
    }

    const auto mediaSsrc = readUint32(buf + 8);
    size_t missedPackets = 0;
    std::vector<std::vector<uint8_t>> retransmissions;
    std::vector<uint16_t> requestedSequences;
    std::unordered_set<uint16_t> uniqueRequestedSequences;
    const auto addRequestedSequence = [&](uint16_t sequence) {
        if (requestedSequences.size() < MAX_NACK_RETRANSMISSIONS
            && uniqueRequestedSequences.insert(sequence).second) {
            requestedSequences.push_back(sequence);
        }
    };
    for (size_t offset = 12; offset + 4 <= feedbackEnd; offset += 4) {
        const auto packetId = static_cast<uint16_t>(buf[offset] << 8 | buf[offset + 1]);
        const auto lostPacketBitmask = static_cast<uint16_t>(buf[offset + 2] << 8 | buf[offset + 3]);
        addRequestedSequence(packetId);
        for (unsigned bit = 0; bit < 16; ++bit) {
            if (lostPacketBitmask & (1u << bit))
                addRequestedSequence(static_cast<uint16_t>(packetId + bit + 1));
        }
    }

    {
        std::lock_guard lock(sentRtpPacketsMutex_);
        const auto now = clock::now();
        while (!sentRtpPackets_.empty()
               && now - sentRtpPackets_.front().sentAt > MAX_CACHED_RTP_PACKET_AGE) {
            sentRtpPackets_.pop_front();
        }

        for (const auto sequence : requestedSequences) {
            const auto packet = std::find_if(
                sentRtpPackets_.rbegin(),
                sentRtpPackets_.rend(),
                [mediaSsrc, sequence, now](const auto& cached) {
                    return cached.ssrc == mediaSsrc && cached.sequence == sequence
                           && now - cached.sentAt <= MAX_CACHED_RTP_PACKET_AGE;
                });
            if (packet != sentRtpPackets_.rend())
                retransmissions.push_back(packet->payload);
            else
                ++missedPackets;
        }
    }

    for (auto& packet : retransmissions) {
        const uint8_t* data = packet.data();
        auto size = static_cast<int>(packet.size());
        std::optional<RtpPacerPacket> pacedPacket;
        if (rtpPacingBitrateBps_.load(std::memory_order_relaxed) != 0) {
            pacedPacket = paceRtpPacket(data, size);
            if (!pacedPacket)
                continue;
            data = pacedPacket->payload.data();
            size = static_cast<int>(pacedPacket->payload.size());
        }

        int sent;
        do {
            sent = writeData(data, size);
        } while (sent < 0 && errno == EAGAIN && !packetState_->interrupted);
        if (sent != size)
            JAMI_WARNING("Failed to retransmit cached RTP packet");
    }
    if (missedPackets != 0 && keyframeRequestCallback_) {
        const auto now = clock::now();
        if (now - lastNackKeyframeRequest_ >= std::chrono::seconds(5)) {
            lastNackKeyframeRequest_ = now;
            keyframeRequestCallback_();
        }
    }
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

std::list<TransportCcFeedback>
SocketPair::getRtcpTransportCc()
{
    std::lock_guard lock(rtcpInfo_mutex_);
    return std::move(listRtcpTransportCc_);
}

std::list<TransportCcReport>
SocketPair::getRtcpTransportCcReports()
{
    std::lock_guard lock(rtcpInfo_mutex_);
    return std::move(listRtcpTransportCcReports_);
}

size_t
SocketPair::getTransportCcSentPacketCount() const
{
    const auto state = packetState_->transportCcState;
    if (!state)
        return 0;

    std::lock_guard<std::mutex> lock(state->mutex);
    return state->sentPackets.size();
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

std::optional<uint16_t>
SocketPair::nextTransportCcSequenceNumber()
{
    const auto state = packetState_->transportCcState;
    if (!state)
        return std::nullopt;

    std::lock_guard l(state->mutex);
    return state->nextSequenceNumber++;
}

void
SocketPair::recordTransportCcSend(uint16_t sequenceNumber, size_t packetSize)
{
    const auto state = packetState_->transportCcState;
    if (!state)
        return;

    std::lock_guard l(state->mutex);
    static constexpr size_t MAX_TRANSPORT_CC_SEND_HISTORY = 4096;
    if (state->sentPackets.size() >= MAX_TRANSPORT_CC_SEND_HISTORY)
        state->sentPackets.erase(state->sentPackets.begin());

    state->sentPackets.push_back({sequenceNumber, std::chrono::steady_clock::now(), packetSize});
}

std::optional<TransportCcReport>
SocketPair::createTransportCcReport(const TransportCcFeedback& feedback)
{
    const auto state = packetState_->transportCcState;
    if (!state)
        return std::nullopt;

    std::vector<TransportCcState::SentPacket> sentPackets;
    {
        std::lock_guard l(state->mutex);
        sentPackets = state->sentPackets;
    }

    if (sentPackets.empty())
        return std::nullopt;

    TransportCcReport report;
    report.feedback = feedback;
    report.feedbackReceiveTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                       std::chrono::steady_clock::now().time_since_epoch())
                                       .count();
    report.packets.reserve(feedback.packets.size());
    std::vector<uint16_t> reportedSequences;
    reportedSequences.reserve(feedback.packets.size());

    bool hasBaseSendTime = false;
    int64_t baseSendTimeUs = 0;
    auto receiveTimeUs = int64_t(feedback.referenceTime) * 64000;
    for (const auto& packet : feedback.packets) {
        const auto sentPacket = std::find_if(sentPackets.begin(), sentPackets.end(), [&packet](const auto& sent) {
            return sent.sequenceNumber == packet.sequenceNumber;
        });
        if (sentPacket == sentPackets.end()) {
            ++report.missingSendHistoryPackets;
            continue;
        }

        const auto sendTimeUs
            = std::chrono::duration_cast<std::chrono::microseconds>(sentPacket->sendTime.time_since_epoch()).count();
        if (!hasBaseSendTime) {
            baseSendTimeUs = sendTimeUs;
            hasBaseSendTime = true;
        }

        TransportCcPacketReport packetReport;
        packetReport.sequenceNumber = packet.sequenceNumber;
        packetReport.status = packet.status;
        packetReport.payloadSize = sentPacket->packetSize;
        packetReport.sendTimeOffsetUs = sendTimeUs - baseSendTimeUs;

        if (packet.status != TransportCcPacketStatus::NotReceived) {
            receiveTimeUs += int64_t(packet.deltaTicks) * TRANSPORT_CC_DELTA_UNIT_US;
            packetReport.receiveTimeOffsetUs = receiveTimeUs;
        }

        report.packets.push_back(packetReport);
        reportedSequences.push_back(packet.sequenceNumber);
    }

    if (report.packets.empty())
        return std::nullopt;

    {
        std::lock_guard l(state->mutex);
        state->sentPackets.erase(std::remove_if(state->sentPackets.begin(),
                                                state->sentPackets.end(),
                                                [&reportedSequences](const auto& sent) {
                                                    return std::find(reportedSequences.begin(),
                                                                     reportedSequences.end(),
                                                                     sent.sequenceNumber)
                                                           != reportedSequences.end();
                                                }),
                                 state->sentPackets.end());
    }

    return report;
}

void
SocketPair::recordTransportCcReceive(const std::shared_ptr<PacketState>& packetState, const uint8_t* buf, size_t len)
{
    std::optional<unsigned> extId;
    std::shared_ptr<TransportCcState> state;
    {
        std::lock_guard l(packetState->dataBuffMutex);
        extId = packetState->transportCcExtId;
        state = packetState->transportCcState;
    }

    if (!extId || !state)
        return;

    const auto value = getOneByteRtpExtensionString(buf, len, *extId);
    if (!value)
        return;

    const auto sequenceNumber = parseTransportCcExtension(reinterpret_cast<const uint8_t*>(value->data()),
                                                          value->size());
    if (!sequenceNumber)
        return;

    std::lock_guard l(state->mutex);
    if (std::any_of(state->receivedPackets.begin(), state->receivedPackets.end(), [sequenceNumber](const auto& packet) {
            return packet.sequenceNumber == *sequenceNumber;
        })) {
        return;
    }

    static constexpr size_t MAX_TRANSPORT_CC_RECEIVE_HISTORY = 512;
    if (state->receivedPackets.size() >= MAX_TRANSPORT_CC_RECEIVE_HISTORY)
        state->receivedPackets.erase(state->receivedPackets.begin());

    state->receivedPackets.push_back({*sequenceNumber, std::chrono::steady_clock::now()});
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

std::vector<uint8_t>
SocketPair::createRtcpTransportCcFeedback(uint32_t senderSsrc, uint32_t mediaSsrc)
{
    const auto state = packetState_->transportCcState;
    if (!state)
        return {};

    std::vector<TransportCcState::ReceivedPacket> receivedPackets;
    uint8_t feedbackPacketCount = 0;
    {
        std::lock_guard l(state->mutex);
        if (state->receivedPackets.empty())
            return {};

        receivedPackets = std::move(state->receivedPackets);
        state->receivedPackets.clear();
        feedbackPacketCount = state->feedbackPacketCount++;
    }

    const auto baseSequenceNumber = receivedPackets.front().sequenceNumber;
    std::sort(receivedPackets.begin(), receivedPackets.end(), [baseSequenceNumber](const auto& lhs, const auto& rhs) {
        return static_cast<uint16_t>(lhs.sequenceNumber - baseSequenceNumber)
               < static_cast<uint16_t>(rhs.sequenceNumber - baseSequenceNumber);
    });

    const auto firstReceiveTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                        receivedPackets.front().receiveTime.time_since_epoch())
                                        .count();
    const auto referenceTimeUs = (firstReceiveTimeUs / 64000) * 64000;

    TransportCcFeedback feedback;
    feedback.senderSsrc = senderSsrc;
    feedback.mediaSsrc = mediaSsrc;
    feedback.baseSequenceNumber = receivedPackets.front().sequenceNumber;
    feedback.referenceTime = static_cast<uint32_t>((firstReceiveTimeUs / 64000) & 0x00ffffff);
    feedback.feedbackPacketCount = feedbackPacketCount;
    feedback.packets.reserve(receivedPackets.size());

    static constexpr size_t MAX_TRANSPORT_CC_FEEDBACK_PACKETS = 512;
    auto expectedSequenceNumber = feedback.baseSequenceNumber;
    auto previousReceiveTimeUs = referenceTimeUs;
    for (const auto& receivedPacket : receivedPackets) {
        while (expectedSequenceNumber != receivedPacket.sequenceNumber
               && feedback.packets.size() < MAX_TRANSPORT_CC_FEEDBACK_PACKETS) {
            feedback.packets.push_back({expectedSequenceNumber, TransportCcPacketStatus::NotReceived, 0});
            ++expectedSequenceNumber;
        }
        if (expectedSequenceNumber != receivedPacket.sequenceNumber
            || feedback.packets.size() >= MAX_TRANSPORT_CC_FEEDBACK_PACKETS) {
            break;
        }

        const auto receiveTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                       receivedPacket.receiveTime.time_since_epoch())
                                       .count();
        auto deltaTicks = (receiveTimeUs - previousReceiveTimeUs) / TRANSPORT_CC_DELTA_UNIT_US;
        deltaTicks = std::clamp<int64_t>(deltaTicks,
                                         std::numeric_limits<int16_t>::min(),
                                         std::numeric_limits<int16_t>::max());

        feedback.packets.push_back({receivedPacket.sequenceNumber,
                                    deltaTicks >= 0 && deltaTicks <= 255 ? TransportCcPacketStatus::SmallDelta
                                                                         : TransportCcPacketStatus::LargeDelta,
                                    static_cast<int16_t>(deltaTicks)});
        previousReceiveTimeUs = receiveTimeUs;
        ++expectedSequenceNumber;
    }

    return jami::createTransportCcFeedbackPacket(feedback);
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
                return AVERROR_EXIT;
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
        return AVERROR_EXIT;
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
    const auto retryOrCancel = [this] {
        return packetState_->interrupted || !packetState_->readBlockingMode ? AVERROR_EXIT : AVERROR(EAGAIN);
    };

    if (rtcpMux_ && rtpHandle_ >= 0 && packetState_->rtpDataBuff.empty() && packetState_->rtcpDataBuff.empty()) {
        auto queued = queueMuxedSocketData();
        if (queued < 0)
            return queued;
    }

    int len = 0;
    bool fromRTCP = false;
    bool authenticatedRtcp = !rtcpProtection_;

    if (datatype & static_cast<int>(DataType::RTCP)) {
        len = readRtcpData(buf, buf_size);
        // SRTCP decrypt (RFC 3711 3.4). Fall back to the packet as-is on
        // failure: legacy peers exchange plaintext RTCP.
        if (len > 0 and rtcpProtection_ and srtpContext_ and srtpContext_->srtp_in.aes) {
            int decryptedLen = len;
            if (ff_srtp_decrypt(&srtpContext_->srtp_in, buf, &decryptedLen) == 0) {
                len = decryptedLen;
                authenticatedRtcp = true;
            } else if (!isCompleteRtcpCompoundPacket(buf, len)) {
                len = 0;
            }
        }
        if (len > 0)
            len = stripRtcpByePackets(buf, len);
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
                // 205 = RTPFB PT: Generic NACK (FMT 1), Transport-CC (FMT 15)
                else if (header->pt == RTCP_PT_RTPFB) {
                    if ((packet[0] & 0x1f) == RTCP_RTPFB_GENERIC_NACK && authenticatedRtcp)
                        retransmitNackPackets(packet, packetSize);
                    else if ((packet[0] & 0x1f) == TRANSPORT_CC_RTCP_FORMAT)
                        saveRtcpTransportCcPacket(packet, packetSize);
                }
                // Sender reports and source descriptions need no local action.
                else if (header->pt != 200 && header->pt != RTCP_PT_SDES) {
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

    if (len < 0)
        return len;
    if (len == 0)
        return retryOrCancel();

    if (!fromRTCP) {
        if (const auto remoteSsrc = getRtpSsrc(buf, static_cast<size_t>(len)); remoteSsrc)
            setRemoteSsrc(*remoteSsrc);
        recordTransportCcReceive(packetState_, buf, static_cast<size_t>(len));
    }

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

        const auto sequence = static_cast<uint16_t>(buf[2] << 8 | buf[3]);
        auto err = ff_srtp_decrypt(&srtpContext_->srtp_in, buf, &len);
        if (err < 0) {
            JAMI_WARNING("decrypt error {}", err);
            return retryOrCancel();
        }

        if (packetLossCallback_ and sequence != static_cast<uint16_t>(lastSeqNumIn_ + 1))
            packetLossCallback_();
        lastSeqNumIn_ = sequence;
    }

    if (len != 0)
        return len;

    return retryOrCancel();
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
    std::array<uint8_t, RTP_MAX_PACKET_LENGTH> firstPatchedPacket {};
    std::array<uint8_t, RTP_MAX_PACKET_LENGTH> secondPatchedPacket {};
    std::vector<uint8_t> encryptedPacket;
    bool useFirstPatchedPacket = true;
    unsigned int ts_LSB, ts_MSB;
    double currentSRTS, currentLatency;

    const auto appendRtpExtension = [&](unsigned extId, std::string_view value) {
        auto* patchedPacket = useFirstPatchedPacket ? firstPatchedPacket.data() : secondPatchedPacket.data();
        const auto patchedSize
            = appendOneByteRtpExtensionString(buf, buf_size, extId, value, patchedPacket, RTP_MAX_PACKET_LENGTH);
        if (patchedSize != buf_size) {
            buf = patchedPacket;
            buf_size = patchedSize;
            useFirstPatchedPacket = !useFirstPatchedPacket;
            return true;
        }
        return false;
    };

    if (isRTCP) {
        if (const auto senderSsrc = getRtcpSenderSsrc(buf, static_cast<size_t>(buf_size)); senderSsrc)
            setLocalSsrc(*senderSsrc);
    } else {
        if (const auto localSsrc = getRtpSsrc(buf, static_cast<size_t>(buf_size)); localSsrc)
            setLocalSsrc(*localSsrc);
    }

    if (!isRTCP && localRtpMidExtId_ && !localRtpMid_.empty()) {
        appendRtpExtension(*localRtpMidExtId_, localRtpMid_);
    }

    std::optional<uint16_t> transportCcSequenceNumber;
    if (!isRTCP && localTransportCcExtId_) {
        if (const auto sequenceNumber = nextTransportCcSequenceNumber()) {
            const auto extension = createTransportCcExtension(*sequenceNumber);
            if (appendRtpExtension(*localTransportCcExtId_,
                                   {reinterpret_cast<const char*>(extension.data()), extension.size()})) {
                transportCcSequenceNumber = *sequenceNumber;
            }
        }
    }

    if (isRTCP && !localRtpMid_.empty()) {
        if (const auto senderSsrc = getRtcpSenderSsrc(buf, static_cast<size_t>(buf_size)); senderSsrc) {
            const auto patchedSize = appendRtcpMidSdesItem(buf,
                                                           buf_size,
                                                           *senderSsrc,
                                                           localRtpMid_,
                                                           firstPatchedPacket.data(),
                                                           firstPatchedPacket.size());
            if (patchedSize != buf_size) {
                buf = firstPatchedPacket.data();
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
        std::lock_guard lock(srtpWriteMutex_);
        const auto encryptedSize = ff_srtp_encrypt(&srtpContext_->srtp_out,
                                                   buf,
                                                   buf_size,
                                                   srtpContext_->encryptbuf,
                                                   sizeof(srtpContext_->encryptbuf));
        if (encryptedSize < 0) {
            JAMI_WARNING("encrypt error {}", encryptedSize);
            return encryptedSize;
        }

        encryptedPacket.assign(srtpContext_->encryptbuf,
                               srtpContext_->encryptbuf + encryptedSize);
        buf = encryptedPacket.data();
        buf_size = encryptedSize;
    }

    std::optional<RtpPacerPacket> pacedPacket;
    if (!isRTCP && rtpPacingBitrateBps_.load(std::memory_order_relaxed) != 0) {
        pacedPacket = paceRtpPacket(buf, buf_size);
        if (!pacedPacket)
            return packetState_->interrupted ? -EINTR : buf_size;
        buf = pacedPacket->payload.data();
        buf_size = static_cast<int>(pacedPacket->payload.size());
    }

    do {
        if (packetState_->interrupted)
            return -EINTR;
        ret = writeData(buf, buf_size);
    } while (ret < 0 and errno == EAGAIN);

    if (ret < 0 && !isRTCP) {
        JAMI_WARNING("Failed to send RTP packet: sequence={}, size={}, error={}",
                     buf_size >= static_cast<int>(RTP_FIXED_HEADER_SIZE)
                         ? static_cast<uint16_t>(buf[2] << 8 | buf[3])
                         : 0,
                     buf_size,
                     ret);
    }
    if (ret > 0 && transportCcSequenceNumber)
        recordTransportCcSend(*transportCcSequenceNumber, static_cast<size_t>(buf_size));
    if (ret > 0 && !isRTCP)
        cacheSentRtpPacket(buf, static_cast<size_t>(buf_size));

    return ret < 0 ? -errno : ret;
}

std::optional<RtpPacerPacket>
SocketPair::paceRtpPacket(const uint8_t* buf, int buf_size)
{
    if (!buf || buf_size <= 0)
        return std::nullopt;

    auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(clock::now().time_since_epoch()).count();
    std::unique_lock lock(rtpPacerMutex_);
    if (!rtpPacer_.enqueue(std::vector<uint8_t>(buf, buf + buf_size), nowUs))
        return std::nullopt;

    while (!packetState_->interrupted) {
        if (auto packet = rtpPacer_.popReady(nowUs))
            return packet;

        const auto nextReadyTimeUs = rtpPacer_.nextReadyTimeUs();
        if (!nextReadyTimeUs)
            return std::nullopt;

        const auto delayUs = std::max<int64_t>(0, *nextReadyTimeUs - nowUs);
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(delayUs));
        nowUs = std::chrono::duration_cast<std::chrono::microseconds>(clock::now().time_since_epoch()).count();
        lock.lock();
    }
    return std::nullopt;
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

void
SocketPair::setTransportCcExtension(std::optional<unsigned> localExtId, std::optional<unsigned> remoteExtId)
{
    localTransportCcExtId_ = (localExtId && *localExtId != 0 && *localExtId < 15) ? localExtId : std::nullopt;

    std::lock_guard lk(packetState_->dataBuffMutex);
    packetState_->transportCcExtId = (remoteExtId && *remoteExtId != 0 && *remoteExtId < 15) ? remoteExtId
                                                                                             : std::nullopt;
}

void
SocketPair::setRtpPacingBitrate(uint64_t bitrateBps)
{
    rtpPacingBitrateBps_.store(bitrateBps, std::memory_order_relaxed);
    std::lock_guard lock(rtpPacerMutex_);
    rtpPacer_.setConfig({bitrateBps, RTP_PACING_MAX_QUEUE_DELAY_US});
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
