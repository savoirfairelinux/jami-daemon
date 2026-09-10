#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace jami {

struct RtpPacerConfig
{
    uint64_t pacingBitrateBps {};
    int64_t maxQueueDelayUs {200'000};
};

struct RtpPacerPacket
{
    std::vector<uint8_t> payload;
    int64_t enqueueTimeUs {};
    int64_t releaseTimeUs {};
};

class RtpPacer
{
public:
    explicit RtpPacer(RtpPacerConfig config = {});

    void setConfig(RtpPacerConfig config);
    bool enqueue(std::vector<uint8_t> payload, int64_t nowUs);
    std::optional<RtpPacerPacket> popReady(int64_t nowUs);
    std::optional<int64_t> nextReadyTimeUs() const;
    void clear();

    bool empty() const { return queue_.empty(); }
    size_t queuedPackets() const { return queue_.size(); }
    size_t queuedBytes() const { return queuedBytes_; }

private:
    int64_t packetSpacingUs(size_t packetBytes) const;

    RtpPacerConfig config_ {};
    std::deque<RtpPacerPacket> queue_ {};
    size_t queuedBytes_ {};
    int64_t nextReleaseTimeUs_ {};
};

} // namespace jami
