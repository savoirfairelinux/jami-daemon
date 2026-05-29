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
{
    // Normalize limits: ensure min <= max
    if (cfg_.minBitrateKbps > cfg_.maxBitrateKbps)
        std::swap(cfg_.minBitrateKbps, cfg_.maxBitrateKbps);

    // Clamp initial bitrate to normalized bounds
    cfg_.initialBitrateKbps = std::clamp(cfg_.initialBitrateKbps, cfg_.minBitrateKbps, cfg_.maxBitrateKbps);
    targetBitrate_ = cfg_.initialBitrateKbps;
    lastStableBitrate_ = cfg_.initialBitrateKbps;

    // Validate EWMA alphas and thresholds
    cfg_.lossAlpha = std::clamp(cfg_.lossAlpha, 0.0f, 1.0f);
    cfg_.rttAlpha = std::clamp(cfg_.rttAlpha, 0.0f, 1.0f);
    cfg_.congestionThreshold = std::clamp(cfg_.congestionThreshold, 0.01f, 1.0f);

    // Validate timing parameters
    if (cfg_.decreaseCooldownSec <= 0.0f)
        cfg_.decreaseCooldownSec = 2.0f;
    if (cfg_.probeDelaySec <= 0.0f)
        cfg_.probeDelaySec = 5.0f;
    if (cfg_.increaseRampTime <= 0.0f)
        cfg_.increaseRampTime = 10.0f;

    // Clamp reduction/increase factors to sane ranges
    cfg_.maxDecreaseFactor = std::clamp(cfg_.maxDecreaseFactor, 0.1f, 1.0f);
    cfg_.audioReductionFactor = std::clamp(cfg_.audioReductionFactor, 0.1f, 1.0f);
    cfg_.minIncreaseFactor = std::clamp(cfg_.minIncreaseFactor, 0.01f, 0.5f);
    cfg_.maxIncreaseFactor = std::clamp(cfg_.maxIncreaseFactor, 0.01f, 0.5f);
    // Ensure min <= max for increase factors
    if (cfg_.minIncreaseFactor > cfg_.maxIncreaseFactor)
        std::swap(cfg_.minIncreaseFactor, cfg_.maxIncreaseFactor);
}

BandwidthController::BandwidthController()
    : BandwidthController(Config {})
{}

void
BandwidthController::reset()
{
    std::lock_guard lock(mutex_);
    ewmaLoss_ = 0.0f;
    ewmaRtt_ = 0.0f;
    ewmaJitter_ = 0.0f;
    baselineRtt_ = 0.0f;
    rttSampleCount_ = 0;
    state_ = State::Stable;
    targetBitrate_ = cfg_.initialBitrateKbps;
    lastStableBitrate_ = cfg_.initialBitrateKbps;
    lastDecrease_ = time_point::min();
    lastIncrease_ = time_point::min();
    lastUpdate_ = time_point::min();
    lastPeriodicLog_ = time_point::min();
}

void
BandwidthController::setLimits(unsigned minKbps, unsigned maxKbps)
{
    std::lock_guard lock(mutex_);
    if (minKbps > maxKbps)
        std::swap(minKbps, maxKbps);
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

void
BandwidthController::syncAppliedBitrate(unsigned bitrateKbps)
{
    std::lock_guard lock(mutex_);
    targetBitrate_ = clampBitrate(bitrateKbps);
    // Do NOT update lastStableBitrate_ — this is the recovery ceiling
    // and should only change on explicit setTargetBitrate() or state transitions.
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
    float lossComponent = (cfg_.congestionLossScale > 0)
                              ? std::max(0.0f, (ewmaLoss_ - cfg_.baselineLoss) / cfg_.congestionLossScale)
                              : 0.0f;
    float rttExcess = (baselineRtt_ > 0) ? std::max(0.0f, ewmaRtt_ - baselineRtt_) : ewmaRtt_;
    float rttComponent = (cfg_.congestionRttScale > 0) ? rttExcess / cfg_.congestionRttScale : 0.0f;
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
    targetBitrate_ = clampBitrate(static_cast<unsigned>(std::lround(static_cast<float>(targetBitrate_) * factor)));

    JAMI_LOG("[BandwidthCtrl] DECREASE: {} -> {} Kbps (congestion={:.2f}, factor={:.2f})",
             oldBitrate,
             targetBitrate_,
             congestion,
             factor);

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

    // Compute ramp progress — if never decreased, use conservative ramp
    float rampProgress = 0.0f;
    if (lastDecrease_ != time_point::min()) {
        auto secSinceDecrease = std::chrono::duration<float>(now - lastDecrease_).count();
        rampProgress = std::clamp((secSinceDecrease - cfg_.probeDelaySec) / cfg_.increaseRampTime, 0.0f, 1.0f);
    }
    float increaseFactor = cfg_.minIncreaseFactor + rampProgress * (cfg_.maxIncreaseFactor - cfg_.minIncreaseFactor);

    // Slow down when approaching the last known stable bitrate to avoid re-triggering congestion
    if (lastStableBitrate_ > 0 && targetBitrate_ >= lastStableBitrate_ * 85 / 100) {
        increaseFactor = std::min(increaseFactor, cfg_.minIncreaseFactor);
    }

    auto oldBitrate = targetBitrate_;
    targetBitrate_ = clampBitrate(
        static_cast<unsigned>(std::lround(static_cast<float>(targetBitrate_) * (1.0f + increaseFactor))));

    if (targetBitrate_ != oldBitrate) {
        JAMI_LOG("[BandwidthCtrl] INCREASE: {} -> {} Kbps (ramp={:.0f}%, +{:.1f}%)",
                 oldBitrate,
                 targetBitrate_,
                 rampProgress * 100,
                 increaseFactor * 100);
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

    // Reject non-finite samples that could permanently poison EWMA state
    if (!std::isfinite(packetLoss) || !std::isfinite(jitterMs) || !std::isfinite(rttMs))
        return targetBitrate_;

    // Clamp inputs to valid ranges
    packetLoss = std::clamp(packetLoss, 0.0f, 100.0f);
    jitterMs = std::max(jitterMs, 0.0f);
    rttMs = std::max(rttMs, 0.0f);

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

    // Establish baseline RTT from first valid samples
    if (rttMs > 0 && rttSampleCount_ < 5) {
        baselineRtt_ = (baselineRtt_ * static_cast<float>(rttSampleCount_) + rttMs)
                       / static_cast<float>(rttSampleCount_ + 1);
        rttSampleCount_++;
    }

    lastUpdate_ = now;

    // Compute unified congestion score
    float congestion = computeCongestionScore();

    // State machine transition
    evaluateState(now, congestion);

    // Periodic status log
    logStatus(now);

    return targetBitrate_;
}

void
BandwidthController::evaluateState(time_point now, float congestion)
{
    auto secSinceDecrease = secSinceLastDecrease(now);

    if (congestion > cfg_.congestionThreshold) {
        if (secSinceDecrease > cfg_.decreaseCooldownSec) {
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
}

void
BandwidthController::logStatus(time_point now)
{
    if (lastPeriodicLog_ == time_point::min() || std::chrono::duration<float>(now - lastPeriodicLog_).count() >= 5.0f) {
        lastPeriodicLog_ = now;
        const char* stateStr = (state_ == State::Stable) ? "STABLE" : (state_ == State::Draining) ? "DRAIN" : "PROBE";
        JAMI_DEBUG("[BandwidthCtrl] bitrate={} Kbps | loss={:.1f}% jitter={:.1f}ms rtt={:.1f}ms "
                   "| state={}",
                   targetBitrate_,
                   ewmaLoss_,
                   ewmaJitter_,
                   ewmaRtt_,
                   stateStr);
    }
}

unsigned
BandwidthController::onAudioCongestion(float audioLoss)
{
    std::lock_guard lock(mutex_);
    auto now = clock::now();

    // Validate input: reject non-finite or negative values
    if (!std::isfinite(audioLoss) || audioLoss < 0.0f)
        return targetBitrate_;

    // Ignore if below threshold
    if (audioLoss < cfg_.audioLossThreshold)
        return targetBitrate_;

    // Respect cooldown
    auto secSinceDecrease = secSinceLastDecrease(now);
    if (secSinceDecrease < cfg_.decreaseCooldownSec)
        return targetBitrate_;

    // Audio congestion: scale reduction from configured factor based on loss severity.
    // Severe loss (>10%) uses the configured factor directly; moderate loss (>5%)
    // uses a softer reduction halfway between configured factor and 1.0.
    float reductionFactor = cfg_.audioReductionFactor;
    if (audioLoss <= 10.0f && audioLoss > 5.0f)
        reductionFactor = (cfg_.audioReductionFactor + 1.0f) * 0.5f;

    // Remember pre-congestion bitrate
    if (state_ != State::Draining)
        lastStableBitrate_ = targetBitrate_;

    auto oldBitrate = targetBitrate_;
    targetBitrate_ = clampBitrate(
        static_cast<unsigned>(std::lround(static_cast<float>(targetBitrate_) * reductionFactor)));

    JAMI_LOG("[BandwidthCtrl] AUDIO CONGESTION: {} -> {} Kbps (audio loss={:.1f}%)",
             oldBitrate,
             targetBitrate_,
             audioLoss);

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
        auto secSinceDecrease = secSinceLastDecrease(now);
        if (secSinceDecrease < 0.5f)
            return targetBitrate_;

        auto oldBitrate = targetBitrate_;
        // Preserve pre-congestion bitrate for recovery (like other decrease paths)
        if (state_ != State::Draining)
            lastStableBitrate_ = targetBitrate_;
        targetBitrate_ = clampBitrate(static_cast<unsigned>(std::lround(static_cast<float>(targetBitrate_) * 0.85f)));

        JAMI_LOG("[BandwidthCtrl] REMB OVERUSE: {} -> {} Kbps", oldBitrate, targetBitrate_);

        lastDecrease_ = now;
        state_ = State::Draining;
    }
    // Normal/underuse REMB: the remote endpoint confirms no congestion.
    // If draining, transition to probing to allow recovery even without RR.
    if (!isOveruse && state_ == State::Draining) {
        auto secSinceDecrease = secSinceLastDecrease(now);
        if (secSinceDecrease > cfg_.probeDelaySec) {
            state_ = State::Probing;
            JAMI_LOG("[BandwidthCtrl] REMB normal -> entering PROBING state");
        }
    }
    // In Probing state, normal REMB acts as an increase trigger (recovery path).
    if (!isOveruse && state_ == State::Probing) {
        applyIncrease();
    }

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
