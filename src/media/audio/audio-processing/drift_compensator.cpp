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
// Data needed on both streams before correcting, in seconds
static constexpr double WARMUP = 10.;
// Time constant to bring the alignment back to its initial value, in seconds
static constexpr double PHASE_TAU = 30.;
static constexpr double MAX_PHASE_CORRECTION = 200e-6;
static constexpr double MAX_CORRECTION = 1000e-6;
// A stream silent for longer than this restarts the estimation, in seconds
static constexpr double MAX_GAP = 1.;
static constexpr double LOG_INTERVAL = 60.;

void
DriftCompensator::LinearFit::add(double t, double value)
{
    if (weight_ == 0) {
        first_ = t;
    } else {
        double decay = std::exp(-(t - last_) / tau_);
        weight_ *= decay;
        varT_ *= decay;
        cov_ *= decay;
    }
    last_ = t;
    weight_ += 1;
    double dt = t - meanT_;
    meanT_ += dt / weight_;
    meanV_ += (value - meanV_) / weight_;
    varT_ += dt * (t - meanT_);
    cov_ += dt * (value - meanV_);
}

void
DriftCompensator::LinearFit::reset()
{
    *this = LinearFit(tau_);
}

DriftCompensator::DriftCompensator(unsigned sampleRate)
    : sampleRate_(sampleRate)
    , playbackFit_(RATE_TAU)
    , recordFit_(RATE_TAU)
{}

double
DriftCompensator::seconds(clock::time_point t)
{
    if (origin_ == clock::time_point {})
        origin_ = t;
    return std::chrono::duration<double>(t - origin_).count();
}

void
DriftCompensator::add(LinearFit& fit, double& count, double t, size_t samples)
{
    if (!fit.empty() && t - fit.last() > MAX_GAP) {
        JAMI_WARNING("[drift] {:.1f}s gap in audio stream, restarting drift estimation", t - fit.last());
        reset();
    }
    count += (double) samples;
    fit.add(t, count);
}

void
DriftCompensator::recorded(clock::time_point now, size_t samples)
{
    add(recordFit_, recordCount_, seconds(now), samples);
}

void
DriftCompensator::played(clock::time_point now, size_t in, size_t out)
{
    auto t = seconds(now);
    add(playbackFit_, playbackCount_, t, in);
    adjustment_ += (double) out - (double) in;
    update(t);
}

void
DriftCompensator::update(double now)
{
    if (playbackFit_.empty() || recordFit_.empty()
        || now - std::max(playbackFit_.first(), recordFit_.first()) < WARMUP) {
        return;
    }
    auto playbackRate = playbackFit_.slope();
    auto recordRate = recordFit_.slope();
    if (playbackRate <= 0 || recordRate <= 0)
        return;

    drift_ = recordRate / playbackRate - 1;
    // Fitted positions are free of the delivery jitter; the adjustment is known exactly
    auto phase = playbackFit_.at(now) - recordFit_.at(now) + adjustment_;
    if (!locked_) {
        locked_ = true;
        targetPhase_ = phase;
    }
    auto phaseError = phase - targetPhase_;
    auto phaseCorrection = std::clamp(-phaseError / (sampleRate_ * PHASE_TAU),
                                      -MAX_PHASE_CORRECTION,
                                      MAX_PHASE_CORRECTION);
    correction_ = std::clamp(drift_ + phaseCorrection, -MAX_CORRECTION, MAX_CORRECTION);

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
    playbackFit_.reset();
    recordFit_.reset();
    playbackCount_ = 0;
    recordCount_ = 0;
    adjustment_ = 0;
    locked_ = false;
}

} // namespace jami
