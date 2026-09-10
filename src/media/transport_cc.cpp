#include "media/transport_cc.h"

#include <limits>

namespace jami {
namespace {

static constexpr uint8_t PACKET_VERSION = 2;
static constexpr uint16_t MAX_RUN_LENGTH = 0x1fff;
static constexpr uint32_t MAX_REFERENCE_TIME = 0x00ffffff;

bool
isValidStatus(TransportCcPacketStatus status)
{
    return status == TransportCcPacketStatus::NotReceived || status == TransportCcPacketStatus::SmallDelta
           || status == TransportCcPacketStatus::LargeDelta;
}

bool
hasDelta(TransportCcPacketStatus status)
{
    return status == TransportCcPacketStatus::SmallDelta || status == TransportCcPacketStatus::LargeDelta;
}

void
writeUint16(std::vector<uint8_t>& packet, uint16_t value)
{
    packet.emplace_back(static_cast<uint8_t>(value >> 8));
    packet.emplace_back(static_cast<uint8_t>(value & 0xff));
}

void
writeUint24(std::vector<uint8_t>& packet, uint32_t value)
{
    packet.emplace_back(static_cast<uint8_t>((value >> 16) & 0xff));
    packet.emplace_back(static_cast<uint8_t>((value >> 8) & 0xff));
    packet.emplace_back(static_cast<uint8_t>(value & 0xff));
}

void
writeUint32(std::vector<uint8_t>& packet, uint32_t value)
{
    packet.emplace_back(static_cast<uint8_t>(value >> 24));
    packet.emplace_back(static_cast<uint8_t>((value >> 16) & 0xff));
    packet.emplace_back(static_cast<uint8_t>((value >> 8) & 0xff));
    packet.emplace_back(static_cast<uint8_t>(value & 0xff));
}

uint16_t
readUint16(const uint8_t* data)
{
    return (uint16_t(data[0]) << 8) | uint16_t(data[1]);
}

int16_t
readInt16(const uint8_t* data)
{
    return static_cast<int16_t>(readUint16(data));
}

uint32_t
readUint24(const uint8_t* data)
{
    return (uint32_t(data[0]) << 16) | (uint32_t(data[1]) << 8) | uint32_t(data[2]);
}

uint32_t
readUint32(const uint8_t* data)
{
    return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | uint32_t(data[3]);
}

bool
validateFeedback(const TransportCcFeedback& feedback)
{
    if (feedback.packets.empty() || feedback.packets.size() > std::numeric_limits<uint16_t>::max())
        return false;
    if (feedback.referenceTime > MAX_REFERENCE_TIME)
        return false;

    for (size_t index = 0; index < feedback.packets.size(); ++index) {
        const auto& packet = feedback.packets[index];
        if (packet.sequenceNumber != static_cast<uint16_t>(feedback.baseSequenceNumber + index))
            return false;
        if (!isValidStatus(packet.status))
            return false;
        if (packet.status == TransportCcPacketStatus::SmallDelta && (packet.deltaTicks < 0 || packet.deltaTicks > 255))
            return false;
    }

    return true;
}

void
appendRunLengthChunk(std::vector<uint8_t>& packet, TransportCcPacketStatus status, uint16_t runLength)
{
    const auto chunk = static_cast<uint16_t>((static_cast<uint16_t>(status) << 13) | runLength);
    writeUint16(packet, chunk);
}

std::optional<TransportCcPacketStatus>
packetStatusFromValue(uint16_t value)
{
    if (value > static_cast<uint16_t>(TransportCcPacketStatus::LargeDelta))
        return std::nullopt;
    return static_cast<TransportCcPacketStatus>(value);
}

bool
appendStatus(std::vector<TransportCcPacketStatus>& statuses, uint16_t packetStatusCount, uint16_t statusValue)
{
    if (statuses.size() >= packetStatusCount)
        return true;

    const auto status = packetStatusFromValue(statusValue);
    if (!status)
        return false;

    statuses.emplace_back(*status);
    return true;
}

} // namespace

std::array<uint8_t, 2>
createTransportCcExtension(uint16_t sequenceNumber)
{
    return {static_cast<uint8_t>(sequenceNumber >> 8), static_cast<uint8_t>(sequenceNumber & 0xff)};
}

std::optional<uint16_t>
parseTransportCcExtension(const uint8_t* data, size_t size)
{
    if (!data || size != 2)
        return std::nullopt;
    return readUint16(data);
}

std::vector<uint8_t>
createTransportCcFeedbackPacket(const TransportCcFeedback& feedback)
{
    std::vector<uint8_t> packet;
    if (!validateFeedback(feedback))
        return packet;

    packet.reserve(20 + feedback.packets.size() * 3);
    packet.emplace_back(static_cast<uint8_t>((PACKET_VERSION << 6) | TRANSPORT_CC_RTCP_FORMAT));
    packet.emplace_back(TRANSPORT_CC_RTCP_PACKET_TYPE);
    writeUint16(packet, 0);
    writeUint32(packet, feedback.senderSsrc);
    writeUint32(packet, feedback.mediaSsrc);
    writeUint16(packet, feedback.baseSequenceNumber);
    writeUint16(packet, static_cast<uint16_t>(feedback.packets.size()));
    writeUint24(packet, feedback.referenceTime);
    packet.emplace_back(feedback.feedbackPacketCount);

    size_t statusIndex = 0;
    while (statusIndex < feedback.packets.size()) {
        const auto status = feedback.packets[statusIndex].status;
        uint16_t runLength = 1;
        while (statusIndex + runLength < feedback.packets.size() && runLength < MAX_RUN_LENGTH
               && feedback.packets[statusIndex + runLength].status == status) {
            ++runLength;
        }
        appendRunLengthChunk(packet, status, runLength);
        statusIndex += runLength;
    }

    for (const auto& packetFeedback : feedback.packets) {
        if (packetFeedback.status == TransportCcPacketStatus::SmallDelta) {
            packet.emplace_back(static_cast<uint8_t>(packetFeedback.deltaTicks));
        } else if (packetFeedback.status == TransportCcPacketStatus::LargeDelta) {
            writeUint16(packet, static_cast<uint16_t>(packetFeedback.deltaTicks));
        }
    }

    while (packet.size() % 4 != 0)
        packet.emplace_back(0);

    const auto length = static_cast<uint16_t>(packet.size() / 4 - 1);
    packet[2] = static_cast<uint8_t>(length >> 8);
    packet[3] = static_cast<uint8_t>(length & 0xff);
    return packet;
}

std::optional<TransportCcFeedback>
parseTransportCcFeedbackPacket(const uint8_t* data, size_t size)
{
    if (!data || size < 20)
        return std::nullopt;

    const auto version = data[0] >> 6;
    const auto format = data[0] & 0x1f;
    if (version != PACKET_VERSION || format != TRANSPORT_CC_RTCP_FORMAT || data[1] != TRANSPORT_CC_RTCP_PACKET_TYPE)
        return std::nullopt;

    const auto packetSize = 4u * (static_cast<size_t>(readUint16(data + 2)) + 1u);
    if (packetSize < 20 || packetSize > size)
        return std::nullopt;

    TransportCcFeedback feedback;
    feedback.senderSsrc = readUint32(data + 4);
    feedback.mediaSsrc = readUint32(data + 8);
    feedback.baseSequenceNumber = readUint16(data + 12);
    const auto packetStatusCount = readUint16(data + 14);
    feedback.referenceTime = readUint24(data + 16);
    feedback.feedbackPacketCount = data[19];

    std::vector<TransportCcPacketStatus> statuses;
    statuses.reserve(packetStatusCount);
    size_t offset = 20;
    while (statuses.size() < packetStatusCount) {
        if (offset + 2 > packetSize)
            return std::nullopt;

        const auto chunk = readUint16(data + offset);
        offset += 2;
        if ((chunk & 0x8000) == 0) {
            const auto statusValue = static_cast<uint16_t>((chunk >> 13) & 0x3);
            const auto runLength = static_cast<uint16_t>(chunk & MAX_RUN_LENGTH);
            if (runLength == 0)
                return std::nullopt;
            for (uint16_t runIndex = 0; runIndex < runLength && statuses.size() < packetStatusCount; ++runIndex) {
                if (!appendStatus(statuses, packetStatusCount, statusValue))
                    return std::nullopt;
            }
        } else if ((chunk & 0x4000) == 0) {
            for (int bitIndex = 13; bitIndex >= 0 && statuses.size() < packetStatusCount; --bitIndex) {
                const auto statusValue = static_cast<uint16_t>((chunk >> bitIndex) & 0x1);
                if (!appendStatus(statuses, packetStatusCount, statusValue))
                    return std::nullopt;
            }
        } else {
            for (int shift = 12; shift >= 0 && statuses.size() < packetStatusCount; shift -= 2) {
                const auto statusValue = static_cast<uint16_t>((chunk >> shift) & 0x3);
                if (!appendStatus(statuses, packetStatusCount, statusValue))
                    return std::nullopt;
            }
        }
    }

    feedback.packets.reserve(statuses.size());
    for (size_t statusIndex = 0; statusIndex < statuses.size(); ++statusIndex) {
        TransportCcPacketFeedback packetFeedback;
        packetFeedback.sequenceNumber = static_cast<uint16_t>(feedback.baseSequenceNumber + statusIndex);
        packetFeedback.status = statuses[statusIndex];

        if (packetFeedback.status == TransportCcPacketStatus::SmallDelta) {
            if (offset + 1 > packetSize)
                return std::nullopt;
            packetFeedback.deltaTicks = static_cast<int16_t>(data[offset++]);
        } else if (packetFeedback.status == TransportCcPacketStatus::LargeDelta) {
            if (offset + 2 > packetSize)
                return std::nullopt;
            packetFeedback.deltaTicks = readInt16(data + offset);
            offset += 2;
        }

        feedback.packets.emplace_back(packetFeedback);
    }

    return feedback;
}

} // namespace jami