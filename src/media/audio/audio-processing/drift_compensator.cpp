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

#include "drift_compensator.h"
#include "logger.h"

#include <algorithm>
#include <cmath>

namespace jami {

// Averaging window of the rate estimation, in seconds
static constexpr double RATE_TAU = 60.;
// Stream startup, excluded from the estimation (buffers filling up), in seconds
static constexpr double SETTLE = 5.;
// Data needed on both streams before correcting, in seconds
static constexpr double WARMUP = 10.;
// Time constant to bring the alignment back to its initial value, in seconds
static constexpr double PHASE_TAU = 15.;
static constexpr double MAX_PHASE_CORRECTION = 200e-6;
static constexpr double MAX_CORRECTION = 1000e-6;
// Time constant of the correction applied, in seconds
static constexpr double SMOOTHING_TAU = 3.;
// A stream silent for longer than this restarts the estimation, in seconds
static constexpr double MAX_GAP = 1.;
// A stream off its fit by more than this (and 4 times its jitter) lost or gained samples, in seconds
static constexpr double MAX_JUMP = 0.05;
// How long the offset must persist to be taken as a discontinuity, in seconds
static constexpr double JUMP_CONFIRM = 1.;
static constexpr double LOG_INTERVAL = 60.;

void
DriftCompensator::LinearFit::add(double t, double value)
{
    if (first_ < 0) {
        first_ = t;
    } else {
        double decay = std::exp(-(t - last_) / tau_);
        weight_ *= decay;
        varT_ *= decay;
        cov_ *= decay;
    }
    if (weight_ == 0)
        segmentStart_ = t;
    last_ = t;
    weight_ += 1;
    double dt = t - meanT_;
    meanT_ += dt / weight_;
    meanV_ += (value - meanV_) / weight_;
    varT_ += dt * (t - meanT_);
    cov_ += dt * (value - meanV_);
}

void
DriftCompensator::LinearFit::split()
{
    // The slope keeps the statistics of previous segments, only the offset restarts
    weight_ = 0;
}

DriftCompensator::DriftCompensator(unsigned sampleRate)
    : sampleRate_(sampleRate)
    , playback_(RATE_TAU)
    , record_(RATE_TAU)
{}

double
DriftCompensator::seconds(clock::time_point t)
{
    if (origin_ == clock::time_point {})
        origin_ = t;
    return std::chrono::duration<double>(t - origin_).count();
}

void
DriftCompensator::add(Stream& stream, double t, size_t samples)
{
    auto& fit = stream.fit;
    if (stream.last >= 0 && t - stream.last > MAX_GAP) {
        JAMI_WARNING("[drift] {:.1f}s gap in audio stream, restarting drift estimation", t - stream.last);
        reset();
    }
    stream.last = t;
    if (stream.start < 0)
        stream.start = t;
    stream.count += (double) samples;
    if (t - stream.start < SETTLE)
        return;
    // Relative to the nominal rate, to keep the regression well conditioned
    auto value = stream.count - sampleRate_ * t;
    if (!fit.empty() && t - fit.first() >= WARMUP) {
        auto residual = value - fit.at(t);
        if (std::abs(residual) > std::max(MAX_JUMP * sampleRate_, 4 * std::sqrt(stream.jitter))) {
            if (stream.outlierStart < 0)
                stream.outlierStart = t;
            if (t - stream.outlierStart < JUMP_CONFIRM)
                return;
            // Samples were lost or inserted: start a new segment, and a new alignment target
            JAMI_WARNING("[drift] ~{:.0f} samples discontinuity in audio stream", residual);
            fit.split();
            locked_ = false;
        } else {
            stream.jitter += (residual * residual - stream.jitter) / 100;
        }
        stream.outlierStart = -1;
    }
    fit.add(t, value);
}

void
DriftCompensator::recorded(clock::time_point now, size_t samples)
{
    add(record_, seconds(now), samples);
}

void
DriftCompensator::played(clock::time_point now, size_t in, size_t out)
{
    auto t = seconds(now);
    add(playback_, t, in);
    adjustment_ += (double) out - (double) in;
    update(t);
}

void
DriftCompensator::update(double now)
{
    const auto& playbackFit = playback_.fit;
    const auto& recordFit = record_.fit;
    if (playbackFit.empty() || recordFit.empty() || now - std::max(playbackFit.first(), recordFit.first()) < WARMUP) {
        return;
    }
    auto playbackRate = sampleRate_ + playbackFit.slope();
    auto recordRate = sampleRate_ + recordFit.slope();
    if (playbackRate <= 0 || recordRate <= 0)
        return;

    drift_ = recordRate / playbackRate - 1;
    // Fitted positions are free of the delivery jitter; the adjustment is known exactly
    auto phase = playbackFit.at(now) - recordFit.at(now) + adjustment_;
    // Lock the target once the offsets of both current segments are well known
    if (!locked_ && now - std::max(playbackFit.segmentStart(), recordFit.segmentStart()) >= WARMUP) {
        locked_ = true;
        targetPhase_ = phase;
    }
    auto phaseError = locked_ ? phase - targetPhase_ : 0.;
    auto phaseCorrection = std::clamp(-phaseError / (sampleRate_ * PHASE_TAU),
                                      -MAX_PHASE_CORRECTION,
                                      MAX_PHASE_CORRECTION);
    auto target = std::clamp(drift_ + phaseCorrection, -MAX_CORRECTION, MAX_CORRECTION);
    // Each capture burst moves the fits a little; smooth the steps it would cause
    correction_ += (target - correction_) * (1 - std::exp(-(now - lastUpdate_) / SMOOTHING_TAU));
    lastUpdate_ = now;

    if (now - lastLog_ >= LOG_INTERVAL) {
        lastLog_ = now;
        JAMI_LOG("[drift] clock drift {:.1f} ppm, correction {:.1f} ppm, alignment error {:.1f} samples",
                 drift_ * 1e6,
                 correction_ * 1e6,
                 phaseError);
    }
}

void
DriftCompensator::reset()
{
    // Keep the current correction: the clocks are unlikely to have changed
    playback_ = Stream(RATE_TAU);
    record_ = Stream(RATE_TAU);
    adjustment_ = 0;
    locked_ = false;
}

} // namespace jami
