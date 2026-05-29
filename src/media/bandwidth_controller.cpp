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

#include "bandwidth_controller.h"
#include "logger.h"

#include <algorithm>
#include <cmath>

namespace jami {

BandwidthController::BandwidthController(const Config& cfg)
    : cfg_(cfg)
    , targetBitrate_(cfg.initialBitrateKbps)
    , lastStableBitrate_(cfg.initialBitrateKbps)
{}

BandwidthController::BandwidthController()
    : BandwidthController(Config{})
{}

void
BandwidthController::reset()
{
    std::lock_guard lock(mutex_);
    ewmaLoss_ = 0.0f;
    ewmaRtt_ = 0.0f;
    ewmaJitter_ = 0.0f;
    state_ = State::Probing;
    targetBitrate_ = cfg_.initialBitrateKbps;
    lastStableBitrate_ = cfg_.initialBitrateKbps;
    lastDecrease_ = time_point::min();
    lastIncrease_ = time_point::min();
    lastUpdate_ = time_point::min();
}

void
BandwidthController::setLimits(unsigned minKbps, unsigned maxKbps)
{
    std::lock_guard lock(mutex_);
    cfg_.minBitrateKbps = minKbps;
    cfg_.maxBitrateKbps = maxKbps;
    targetBitrate_ = clampBitrate(targetBitrate_);
}

void
BandwidthController::setTargetBitrate(unsigned bitrateKbps)
{
    std::lock_guard lock(mutex_);
    targetBitrate_ = clampBitrate(bitrateKbps);
    lastStableBitrate_ = targetBitrate_;
}

unsigned
BandwidthController::clampBitrate(unsigned br) const
{
    return std::clamp(br, cfg_.minBitrateKbps, cfg_.maxBitrateKbps);
}

float
BandwidthController::computeCongestionScore() const
{
    // Normalized congestion score [0, 1] combining loss and RTT signals
    float lossComponent = std::max(0.0f, (ewmaLoss_ - cfg_.baselineLoss) / cfg_.congestionLossScale);
    float rttComponent = std::max(0.0f, ewmaRtt_ / cfg_.congestionRttScale);
    // Weighted combination: loss is primary signal, RTT is secondary
    return std::clamp(lossComponent * 0.7f + rttComponent * 0.3f, 0.0f, 1.0f);
}

void
BandwidthController::applyDecrease(float congestion)
{
    // Remember pre-congestion bitrate as reference for recovery ceiling
    if (state_ != State::Draining) {
        lastStableBitrate_ = targetBitrate_;
    }

    // Multiplicative decrease proportional to congestion severity
    // congestion=0.1 → factor=0.95, congestion=1.0 → factor=0.5
    float factor = 1.0f - congestion * cfg_.maxDecreaseFactor;
    factor = std::max(factor, 1.0f - cfg_.maxDecreaseFactor);

    auto oldBitrate = targetBitrate_;
    targetBitrate_ = clampBitrate(
        static_cast<unsigned>(std::lround(static_cast<float>(targetBitrate_) * factor)));

    JAMI_LOG("[BandwidthCtrl] DECREASE: {} -> {} Kbps (congestion={:.2f}, factor={:.2f})",
             oldBitrate, targetBitrate_, congestion, factor);

    lastDecrease_ = clock::now();
    state_ = State::Draining;
}

void
BandwidthController::applyIncrease()
{
    auto now = clock::now();

    // Enforce minimum interval between increases
    if (lastIncrease_ != time_point::min()) {
        auto secSinceIncrease = std::chrono::duration<float>(now - lastIncrease_).count();
        if (secSinceIncrease < MIN_INCREASE_INTERVAL_SEC)
            return;
    }

    auto secSinceDecrease = std::chrono::duration<float>(now - lastDecrease_).count();

    // Increase rate ramps up over time: starts at minIncreaseFactor, reaches maxIncreaseFactor
    // after increaseRampTime seconds of stability
    float rampProgress = std::clamp(
        (secSinceDecrease - cfg_.probeDelaySec) / cfg_.increaseRampTime, 0.0f, 1.0f);
    float increaseFactor = cfg_.minIncreaseFactor
        + rampProgress * (cfg_.maxIncreaseFactor - cfg_.minIncreaseFactor);

    // Slow down when approaching the last known stable bitrate to avoid re-triggering congestion
    if (lastStableBitrate_ > 0 && targetBitrate_ >= lastStableBitrate_ * 85 / 100) {
        increaseFactor = std::min(increaseFactor, cfg_.minIncreaseFactor);
    }

    auto oldBitrate = targetBitrate_;
    targetBitrate_ = clampBitrate(
        static_cast<unsigned>(std::lround(static_cast<float>(targetBitrate_) * (1.0f + increaseFactor))));

    if (targetBitrate_ != oldBitrate) {
        JAMI_LOG("[BandwidthCtrl] INCREASE: {} -> {} Kbps (ramp={:.0f}%, +{:.1f}%)",
                 oldBitrate, targetBitrate_, rampProgress * 100, increaseFactor * 100);
    }

    lastIncrease_ = now;

    // Reached stable state when close to max
    if (targetBitrate_ >= cfg_.maxBitrateKbps * 95 / 100) {
        state_ = State::Stable;
        lastStableBitrate_ = targetBitrate_;
    }
}

unsigned
BandwidthController::update(float packetLoss, float jitterMs, float rttMs)
{
    std::lock_guard lock(mutex_);
    auto now = clock::now();

    // Update EWMA signals
    if (lastUpdate_ == time_point::min()) {
        ewmaLoss_ = packetLoss;
        ewmaRtt_ = rttMs;
        ewmaJitter_ = jitterMs;
    } else {
        ewmaLoss_ = cfg_.lossAlpha * packetLoss + (1.0f - cfg_.lossAlpha) * ewmaLoss_;
        ewmaRtt_ = cfg_.rttAlpha * rttMs + (1.0f - cfg_.rttAlpha) * ewmaRtt_;
        ewmaJitter_ = 0.2f * jitterMs + 0.8f * ewmaJitter_;
    }
    lastUpdate_ = now;

    // Compute unified congestion score
    float congestion = computeCongestionScore();

    // Decision logic based on state
    auto secSinceDecrease = std::chrono::duration<float>(now - lastDecrease_).count();

    if (congestion > cfg_.congestionThreshold) {
        if (secSinceDecrease > cfg_.decreaseCooldownSec || lastDecrease_ == time_point::min()) {
            applyDecrease(congestion);
        }
    } else if (state_ == State::Draining) {
        if (secSinceDecrease > cfg_.probeDelaySec) {
            state_ = State::Probing;
            JAMI_LOG("[BandwidthCtrl] Entering PROBING state (stable for {:.1f}s)", secSinceDecrease);
        }
    } else if (state_ == State::Probing) {
        applyIncrease();
    } else if (state_ == State::Stable) {
        lastStableBitrate_ = targetBitrate_;
    }

    // Periodic status log every 5 seconds
    if (lastPeriodicLog_ == time_point::min()
        || std::chrono::duration<float>(now - lastPeriodicLog_).count() >= 5.0f) {
        lastPeriodicLog_ = now;
        const char* stateStr = (state_ == State::Stable) ? "STABLE"
                             : (state_ == State::Draining) ? "DRAIN"
                             : "PROBE";
        JAMI_LOG("[BandwidthCtrl] bitrate={} Kbps | loss={:.1f}% jitter={:.1f}ms rtt={:.1f}ms | state={}",
                 targetBitrate_, ewmaLoss_, ewmaJitter_, ewmaRtt_, stateStr);
    }

    return targetBitrate_;
}

unsigned
BandwidthController::onAudioCongestion(float audioLoss)
{
    std::lock_guard lock(mutex_);
    auto now = clock::now();

    // Respect cooldown
    auto secSinceDecrease = std::chrono::duration<float>(now - lastDecrease_).count();
    if (secSinceDecrease < cfg_.decreaseCooldownSec && lastDecrease_ != time_point::min())
        return targetBitrate_;

    // Audio congestion: proportional reduction based on audio loss severity
    float reductionFactor = cfg_.audioReductionFactor;
    if (audioLoss > 10.0f)
        reductionFactor = 0.45f;
    else if (audioLoss > 5.0f)
        reductionFactor = 0.55f;

    auto oldBitrate = targetBitrate_;
    targetBitrate_ = clampBitrate(
        static_cast<unsigned>(std::lround(static_cast<float>(targetBitrate_) * reductionFactor)));

    JAMI_LOG("[BandwidthCtrl] AUDIO CONGESTION: {} -> {} Kbps (audio loss={:.1f}%)",
             oldBitrate, targetBitrate_, audioLoss);

    lastDecrease_ = now;
    state_ = State::Draining;

    return targetBitrate_;
}

unsigned
BandwidthController::onRembFeedback(bool isOveruse)
{
    std::lock_guard lock(mutex_);
    auto now = clock::now();

    if (isOveruse) {
        // REMB overuse: moderate decrease (less aggressive than loss-based)
        auto secSinceDecrease = std::chrono::duration<float>(now - lastDecrease_).count();
        if (secSinceDecrease < 0.5f && lastDecrease_ != time_point::min())
            return targetBitrate_;

        auto oldBitrate = targetBitrate_;
        targetBitrate_ = clampBitrate(
            static_cast<unsigned>(std::lround(static_cast<float>(targetBitrate_) * 0.85f)));

        JAMI_LOG("[BandwidthCtrl] REMB OVERUSE: {} -> {} Kbps", oldBitrate, targetBitrate_);

        lastDecrease_ = now;
        state_ = State::Draining;
    }
    // Normal/underuse REMB doesn't trigger increase directly —
    // the regular update() probing handles that more smoothly.

    return targetBitrate_;
}

unsigned
BandwidthController::getTargetBitrate() const
{
    std::lock_guard lock(mutex_);
    return targetBitrate_;
}

BandwidthController::State
BandwidthController::getState() const
{
    std::lock_guard lock(mutex_);
    return state_;
}

} // namespace jami
