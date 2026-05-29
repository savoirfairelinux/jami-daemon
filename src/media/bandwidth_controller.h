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

#include <chrono>
#include <cstdint>
#include <limits>
#include <mutex>

namespace jami {

/**
 * Unified bandwidth controller for video RTP sessions.
 *
 * Replaces the previous fragmented approach (3 independent loops: REMB, RR loss, audio QoS)
 * with a single AIMD-based controller that fuses all congestion signals into a normalized
 * score and applies a coherent increase/decrease strategy.
 *
 * Design principles:
 *  - Single control point: one place decides the video bitrate.
 *  - EWMA-smoothed signals: avoids over-reaction to transient spikes.
 *  - Fast multiplicative decrease, accelerating additive increase (AIMD variant).
 *  - Audio protection: audio loss signal triggers immediate video reduction.
 *  - Baseline-aware: distinguishes random WiFi loss from actual congestion.
 */
class BandwidthController
{
public:
    struct Config
    {
        unsigned minBitrateKbps {50};
        unsigned maxBitrateKbps {4000};
        unsigned initialBitrateKbps {800};
        // EWMA time constants
        float lossAlpha {0.3f}; // Weight of new loss sample (higher = more reactive)
        float rttAlpha {0.2f};  // Weight of new RTT sample
        // Congestion detection
        float baselineLoss {1.5f};         // Expected background loss (%) on WiFi
        float congestionLossScale {12.0f}; // Loss range mapped to [0,1]
        float congestionRttScale {300.0f}; // RTT excess (ms) mapped to [0,1]
        float congestionThreshold {0.08f}; // Below this, no action needed
        // Decrease parameters
        float maxDecreaseFactor {0.5f}; // Maximum single-step decrease (50%)
        // Increase parameters
        float minIncreaseFactor {0.03f}; // Minimum increase per step (3%)
        float maxIncreaseFactor {0.15f}; // Maximum increase per step (15%)
        float increaseRampTime {8.0f};   // Seconds to reach max increase rate
        // Audio protection
        float audioLossThreshold {3.0f};    // Audio loss (%) triggering video reduction
        float audioReductionFactor {0.65f}; // Immediate reduction when audio is congested
        // Timing
        float decreaseCooldownSec {1.5f}; // Minimum time between decreases
        float probeDelaySec {2.0f};       // Time to wait after decrease before probing up
    };

    enum class State {
        Stable,   // Bitrate is at or near target, no action needed
        Draining, // Recently decreased, waiting before probing
        Probing   // Gradually increasing bitrate
    };

    explicit BandwidthController(const Config& cfg);
    BandwidthController();

    /**
     * Main update function. Call every RTCP interval (1-4s) with latest measurements.
     * @param packetLoss Current packet loss percentage [0-100]
     * @param jitterMs   Current jitter in milliseconds
     * @param rttMs      Current round-trip time in milliseconds
     * @return New target bitrate in Kbps
     */
    unsigned update(float packetLoss, float jitterMs, float rttMs);

    /**
     * Called when audio session detects congestion.
     * Triggers immediate video bitrate reduction.
     * @param audioLoss Audio packet loss percentage
     * @return New target bitrate in Kbps
     */
    unsigned onAudioCongestion(float audioLoss);

    /**
     * Process REMB feedback from receiver (delay-based signal).
     * @param isOveruse true if receiver detected overuse
     * @return New target bitrate in Kbps
     */
    unsigned onRembFeedback(bool isOveruse);

    /** Get current target bitrate in Kbps */
    unsigned getTargetBitrate() const;

    /** Get current state */
    State getState() const;

    /** Reset controller (e.g., after call hold/resume) */
    void reset();

    /** Reconfigure limits (e.g., codec change) */
    void setLimits(unsigned minKbps, unsigned maxKbps);

    /** Set current target bitrate (e.g., sync with codec negotiated bitrate) */
    void setTargetBitrate(unsigned bitrateKbps);

    /** Sync controller with the bitrate actually applied by the encoder,
     *  without resetting the recovery ceiling (lastStableBitrate_). */
    void syncAppliedBitrate(unsigned bitrateKbps);

private:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    // Returns seconds since last decrease, or infinity if never decreased
    float secSinceLastDecrease(time_point now) const
    {
        if (lastDecrease_ == time_point::min())
            return std::numeric_limits<float>::infinity();
        return std::chrono::duration<float>(now - lastDecrease_).count();
    }

    float computeCongestionScore() const;
    void evaluateState(time_point now, float congestion);
    void logStatus(time_point now);
    void applyDecrease(float congestion);
    void applyIncrease();
    unsigned clampBitrate(unsigned br) const;

    Config cfg_;
    mutable std::mutex mutex_;

    // Smoothed signals
    float ewmaLoss_ {0.0f};
    float ewmaRtt_ {0.0f};
    float ewmaJitter_ {0.0f};

    // RTT baseline (average of first valid samples)
    float baselineRtt_ {0.0f};
    int rttSampleCount_ {0};

    // State — starts Stable so healthy calls don't ramp on first RTCP
    State state_ {State::Stable};
    unsigned targetBitrate_;
    unsigned lastStableBitrate_;

    // Timing
    time_point lastDecrease_ {time_point::min()};
    time_point lastIncrease_ {time_point::min()};
    time_point lastUpdate_ {time_point::min()};
    time_point lastPeriodicLog_ {time_point::min()};

    // Minimum interval between increases to decouple from polling frequency
    static constexpr float MIN_INCREASE_INTERVAL_SEC {1.0f};
};

} // namespace jami
