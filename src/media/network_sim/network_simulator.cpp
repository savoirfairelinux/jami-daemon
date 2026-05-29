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
    packetLossRatio_ = std::clamp(ratio, 0.0f, 1.0f);
}

void
NetworkSimulator::setBandwidthLimit(uint64_t bitsPerSecond)
{
    std::lock_guard lock(bucketMutex_);
    bandwidthLimit_ = bitsPerSecond;
    // Reset bucket on config change
    tokenBucket_ = bitsPerSecond / 8; // 1 second worth of bytes
    lastRefill_ = std::chrono::steady_clock::now();
}

void
NetworkSimulator::setJitter(uint32_t maxJitterMs)
{
    maxJitterMs_ = maxJitterMs;
}

bool
NetworkSimulator::shouldSend(size_t packetSize)
{
    if (!enabled_)
        return true;

    // Apply jitter (sleep)
    uint32_t jitter = maxJitterMs_.load();
    if (jitter > 0) {
        auto delay = randomJitter();
        if (delay > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }

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
NetworkSimulator::refillBucket()
{
    // Must be called with bucketMutex_ held
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastRefill_);
    lastRefill_ = now;

    uint64_t bwLimit = bandwidthLimit_.load();
    // bytes to add = (bitsPerSec / 8) * elapsed_seconds
    uint64_t bytesToAdd = (bwLimit * static_cast<uint64_t>(elapsed.count())) / (8 * 1'000'000);
    // Cap bucket at 2 seconds worth (allows small bursts)
    uint64_t maxBucket = (bwLimit / 8) * 2;
    tokenBucket_ = std::min(tokenBucket_ + bytesToAdd, maxBucket);
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

uint32_t
NetworkSimulator::randomJitter()
{
    uint32_t maxJ = maxJitterMs_.load();
    if (maxJ == 0)
        return 0;
    std::lock_guard lock(rngMutex_);
    std::uniform_int_distribution<uint32_t> dist(0, maxJ);
    return dist(rng_);
}

} // namespace jami
