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
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>

namespace jami {

/**
 * Network condition simulator for RTP/RTCP packets.
 *
 * Provides configurable packet loss, bandwidth limiting, and jitter
 * injection. Used to test QoS strategies (audio priority over video)
 * under degraded network conditions.
 *
 * Thread-safe: parameters can be changed at any time from any thread.
 */
class NetworkSimulator
{
public:
    struct Stats
    {
        uint64_t packetsSent {0};
        uint64_t packetsDropped {0};
        uint64_t bytesSent {0};
        uint64_t bytesDropped {0};
    };

    NetworkSimulator();

    /**
     * Enable/disable the simulator. When disabled, all packets pass through.
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    /**
     * Set packet loss ratio (0.0 = no loss, 1.0 = all dropped).
     */
    void setPacketLoss(float ratio);

    /**
     * Set maximum bandwidth in bits per second.
     * Packets exceeding the bandwidth budget are dropped.
     * 0 = unlimited.
     */
    void setBandwidthLimit(uint64_t bitsPerSecond);

    /**
     * Set jitter (random delay) range in milliseconds.
     * Actual delay = uniform_random(0, maxJitterMs).
     * 0 = no jitter (no delay injected; packet passes immediately).
     *
     * Note: jitter is simulated via busy-wait/sleep before returning
     * from shouldSend(). For a real pacer, a queue-based approach is better.
     */
    void setJitter(uint32_t maxJitterMs);

    /**
     * Decide whether a packet should be sent.
     * If jitter > 0, this call may block for a random delay.
     *
     * @param packetSize Size in bytes of the packet about to be sent.
     * @return true if the packet should be transmitted, false if dropped.
     */
    bool shouldSend(size_t packetSize);

    /**
     * Reset statistics counters.
     */
    void resetStats();

    /**
     * Get current statistics (thread-safe snapshot).
     */
    Stats getStats() const;

    /**
     * Get packet loss ratio actually observed (dropped / total).
     */
    float getObservedPacketLoss() const;

private:
    std::atomic_bool enabled_ {false};
    std::atomic<float> packetLossRatio_ {0.0f};
    std::atomic<uint64_t> bandwidthLimit_ {0};
    std::atomic<uint32_t> maxJitterMs_ {0};

    // Token bucket for bandwidth limiting
    mutable std::mutex bucketMutex_;
    uint64_t tokenBucket_ {0};
    std::chrono::steady_clock::time_point lastRefill_;

    // Statistics
    mutable std::mutex statsMutex_;
    Stats stats_;

    // Random number generation (per-instance, thread-safe via mutex)
    mutable std::mutex rngMutex_;
    std::mt19937 rng_;

    void refillBucket();
    float randomFloat();
    uint32_t randomJitter();
};

} // namespace jami
