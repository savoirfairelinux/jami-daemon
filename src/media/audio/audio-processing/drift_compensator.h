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
#include <cstddef>

namespace jami {

/**
 * @brief Tracks the clock drift between the capture and playback streams of an audio processor.
 *
 * The rate of each stream is estimated by an exponentially weighted linear regression of its
 * cumulative sample count against a monotonic clock, which averages out the bursty delivery of
 * device callbacks. The resulting correction is meant to resample the playback stream (the echo
 * reference) so that its alignment with the capture stream stays constant.
 */
class DriftCompensator
{
public:
    using clock = std::chrono::steady_clock;

    explicit DriftCompensator(unsigned sampleRate);

    /**
     * @brief Account for @samples capture samples queued at @now.
     */
    void recorded(clock::time_point now, size_t samples);

    /**
     * @brief Account for @in playback samples received, queued as @out samples after correction.
     */
    void played(clock::time_point now, size_t in, size_t out);

    /**
     * @brief Ratio to apply to the playback stream (output / input samples), minus one.
     */
    double correction() const { return correction_; }

    /**
     * @brief Estimated capture / playback clock ratio minus one, 0 until estimated.
     */
    double drift() const { return drift_; }

private:
    class LinearFit
    {
    public:
        explicit LinearFit(double tau)
            : tau_(tau)
        {}
        void add(double t, double value);
        void reset();
        bool empty() const { return weight_ == 0; }
        double first() const { return first_; }
        double last() const { return last_; }
        double slope() const { return varT_ > 0 ? cov_ / varT_ : 0; }
        double at(double t) const { return meanV_ + slope() * (t - meanT_); }

    private:
        double tau_;
        double weight_ {0};
        double meanT_ {0};
        double meanV_ {0};
        double varT_ {0};
        double cov_ {0};
        double first_ {0};
        double last_ {0};
    };

    double seconds(clock::time_point t);
    void add(LinearFit& fit, double& count, double t, size_t samples);
    void update(double now);
    void reset();

    const double sampleRate_;
    clock::time_point origin_ {};
    LinearFit playbackFit_;
    LinearFit recordFit_;
    double playbackCount_ {0};
    double recordCount_ {0};
    // Samples added (or removed, if negative) to the playback stream by the correction
    double adjustment_ {0};
    bool locked_ {false};
    double targetPhase_ {0};
    double drift_ {0};
    double correction_ {0};
    double lastLog_ {0};
};

} // namespace jami
