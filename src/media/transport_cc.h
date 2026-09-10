#ifndef JAMI_MEDIA_TRANSPORT_CC_H
#define JAMI_MEDIA_TRANSPORT_CC_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace jami {

static constexpr uint8_t TRANSPORT_CC_RTCP_FORMAT = 15;
static constexpr uint8_t TRANSPORT_CC_RTCP_PACKET_TYPE = 205;
static constexpr uint16_t TRANSPORT_CC_DELTA_UNIT_US = 250;

enum class TransportCcPacketStatus : uint8_t { NotReceived = 0, SmallDelta = 1, LargeDelta = 2 };

struct TransportCcPacketFeedback
{
    uint16_t sequenceNumber {};
    TransportCcPacketStatus status {TransportCcPacketStatus::NotReceived};
    int16_t deltaTicks {};
};

struct TransportCcFeedback
{
    uint32_t senderSsrc {};
    uint32_t mediaSsrc {};
    uint16_t baseSequenceNumber {};
    uint32_t referenceTime {};
    uint8_t feedbackPacketCount {};
    std::vector<TransportCcPacketFeedback> packets {};
};

struct TransportCcPacketReport
{
    uint16_t sequenceNumber {};
    TransportCcPacketStatus status {TransportCcPacketStatus::NotReceived};
    size_t payloadSize {};
    int64_t sendTimeOffsetUs {};
    int64_t receiveTimeOffsetUs {};
};

struct TransportCcReport
{
    TransportCcFeedback feedback {};
    int64_t feedbackReceiveTimeUs {};
    size_t missingSendHistoryPackets {};
    std::vector<TransportCcPacketReport> packets {};
};

std::array<uint8_t, 2> createTransportCcExtension(uint16_t sequenceNumber);
std::optional<uint16_t> parseTransportCcExtension(const uint8_t* data, size_t size);

std::vector<uint8_t> createTransportCcFeedbackPacket(const TransportCcFeedback& feedback);
std::optional<TransportCcFeedback> parseTransportCcFeedbackPacket(const uint8_t* data, size_t size);

} // namespace jami

#endif // JAMI_MEDIA_TRANSPORT_CC_H