#include "rtp_pacer.h"

#include <algorithm>

namespace jami {

RtpPacer::RtpPacer(RtpPacerConfig config)
    : config_(config)
{}

void
RtpPacer::setConfig(RtpPacerConfig config)
{
    config_ = config;
}

bool
RtpPacer::enqueue(std::vector<uint8_t> payload, int64_t nowUs)
{
    if (payload.empty())
        return true;

    const auto packetBytes = payload.size();
    auto releaseTimeUs = nowUs;
    if (config_.pacingBitrateBps != 0) {
        releaseTimeUs = std::max(nowUs, nextReleaseTimeUs_);
        // Dropping packets from the middle of an encoded frame corrupts it
        // and forces a keyframe round-trip, so bound the queue delay by
        // draining faster instead of dropping (keyframe bursts routinely
        // exceed the average pacing rate).
        if (config_.maxQueueDelayUs >= 0 && releaseTimeUs - nowUs > config_.maxQueueDelayUs)
            releaseTimeUs = nowUs + config_.maxQueueDelayUs;
        nextReleaseTimeUs_ = releaseTimeUs + packetSpacingUs(packetBytes);
    }

    queuedBytes_ += packetBytes;
    queue_.push_back({std::move(payload), nowUs, releaseTimeUs});
    return true;
}

std::optional<RtpPacerPacket>
RtpPacer::popReady(int64_t nowUs)
{
    if (queue_.empty() || queue_.front().releaseTimeUs > nowUs)
        return std::nullopt;

    auto packet = std::move(queue_.front());
    queue_.pop_front();
    queuedBytes_ -= packet.payload.size();
    return packet;
}

std::optional<int64_t>
RtpPacer::nextReadyTimeUs() const
{
    if (queue_.empty())
        return std::nullopt;
    return queue_.front().releaseTimeUs;
}

void
RtpPacer::clear()
{
    queue_.clear();
    queuedBytes_ = 0;
    nextReleaseTimeUs_ = 0;
}

int64_t
RtpPacer::packetSpacingUs(size_t packetBytes) const
{
    if (config_.pacingBitrateBps == 0)
        return 0;

    constexpr auto microsPerSecond = uint64_t(1'000'000);
    const auto packetBits = static_cast<uint64_t>(packetBytes) * 8;
    const auto numerator = packetBits * microsPerSecond;
    return static_cast<int64_t>((numerator + config_.pacingBitrateBps - 1) / config_.pacingBitrateBps);
}

} // namespace jami
