/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#include "network_simulator.h"

#include <algorithm>
#include <thread>

namespace jami {

NetworkSimulator::NetworkSimulator()
    : lastRefill_(std::chrono::steady_clock::now())
    , rng_(std::random_device {}())
{}

void
NetworkSimulator::setPacketLoss(float ratio)
{
    packetLossRatio_.store(std::clamp(ratio, 0.0f, 1.0f), std::memory_order_relaxed);
}

void
NetworkSimulator::setBandwidthLimit(uint64_t bitsPerSecond)
{
    std::lock_guard lock(bucketMutex_);
    bandwidthLimit_ = bitsPerSecond;
    // Reset bucket on config change
    tokenBucket_ = 0;
    lastRefill_ = std::chrono::steady_clock::now();
}

bool
NetworkSimulator::shouldSend(size_t packetSize)
{
    if (!enabled_)
        return true;

    // Note: jitter simulation is not applied here to avoid blocking the RTP send thread.
    // Use namespace-based netem (tc qdisc) for realistic delay/jitter simulation.

    // Apply random packet loss
    float lossRatio = packetLossRatio_.load();
    if (lossRatio > 0.0f && randomFloat() < lossRatio) {
        std::lock_guard lock(statsMutex_);
        stats_.packetsDropped++;
        stats_.bytesDropped += packetSize;
        return false;
    }

    // Apply bandwidth limiting (token bucket)
    uint64_t bwLimit = bandwidthLimit_.load();
    if (bwLimit > 0) {
        std::lock_guard lock(bucketMutex_);
        refillBucket();
        if (tokenBucket_ < packetSize) {
            std::lock_guard slock(statsMutex_);
            stats_.packetsDropped++;
            stats_.bytesDropped += packetSize;
            return false;
        }
        tokenBucket_ -= packetSize;
    }

    // Packet passes
    {
        std::lock_guard lock(statsMutex_);
        stats_.packetsSent++;
        stats_.bytesSent += packetSize;
    }
    return true;
}

void
NetworkSimulator::accountBytes(size_t bytes)
{
    if (!enabled_)
        return;
    uint64_t bwLimit = bandwidthLimit_.load();
    if (bwLimit > 0) {
        std::lock_guard lock(bucketMutex_);
        refillBucket();
        if (tokenBucket_ >= bytes)
            tokenBucket_ -= bytes;
        else
            tokenBucket_ = 0;
    }
}

void
NetworkSimulator::refillBucket()
{
    // Must be called with bucketMutex_ held
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - lastRefill_);
    lastRefill_ = now;

    uint64_t bwLimit = bandwidthLimit_.load();
    // bytes to add = (bitsPerSec / 8) * elapsed_seconds
    auto bytesToAdd = static_cast<uint64_t>((static_cast<double>(bwLimit) / 8.0) * elapsed.count());
    // Cap bucket at 2 seconds worth (allows small bursts)
    uint64_t maxBucket = (bwLimit / 8) * 2;
    auto room = maxBucket > tokenBucket_ ? maxBucket - tokenBucket_ : uint64_t(0);
    tokenBucket_ += std::min(bytesToAdd, room);
}

void
NetworkSimulator::resetStats()
{
    std::lock_guard lock(statsMutex_);
    stats_ = {};
}

NetworkSimulator::Stats
NetworkSimulator::getStats() const
{
    std::lock_guard lock(statsMutex_);
    return stats_;
}

float
NetworkSimulator::getObservedPacketLoss() const
{
    std::lock_guard lock(statsMutex_);
    uint64_t total = stats_.packetsSent + stats_.packetsDropped;
    if (total == 0)
        return 0.0f;
    return static_cast<float>(stats_.packetsDropped) / static_cast<float>(total);
}

float
NetworkSimulator::randomFloat()
{
    std::lock_guard lock(rngMutex_);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng_);
}

} // namespace jami
