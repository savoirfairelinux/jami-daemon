/*
 * Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace jami::video {

class BitrateReconfigurePolicy
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    BitrateReconfigurePolicy() = default;

    BitrateReconfigurePolicy(unsigned bitrate, TimePoint now) { reset(bitrate, now); }

    void reset(unsigned bitrate, TimePoint now)
    {
        appliedBitrate_ = bitrate;
        targetBitrate_ = bitrate;
        lastTargetChange_ = now;
    }

    std::optional<unsigned> update(unsigned target, TimePoint now)
    {
        if (appliedBitrate_ == 0) {
            reset(target, now);
            return target;
        }

        const bool targetChanged = target != targetBitrate_;
        if (targetChanged) {
            targetBitrate_ = target;
            lastTargetChange_ = now;
        }

        if (target == appliedBitrate_)
            return std::nullopt;

        if (target < appliedBitrate_) {
            const auto decrease = static_cast<uint64_t>(appliedBitrate_ - target) * 100;
            if (decrease < static_cast<uint64_t>(appliedBitrate_) * DECREASE_PERCENT)
                return std::nullopt;
        } else {
            const auto increase = static_cast<uint64_t>(target) * 100;
            const auto threshold = static_cast<uint64_t>(appliedBitrate_) * INCREASE_PERCENT;
            if (increase < threshold && (targetChanged || now - lastTargetChange_ < STABILITY_DELAY))
                return std::nullopt;
        }

        appliedBitrate_ = target;
        return target;
    }

    unsigned appliedBitrate() const { return appliedBitrate_; }

private:
    static constexpr uint64_t DECREASE_PERCENT {5};
    static constexpr uint64_t INCREASE_PERCENT {150};
    static constexpr auto STABILITY_DELAY = std::chrono::seconds(6);

    unsigned appliedBitrate_ {0};
    unsigned targetBitrate_ {0};
    TimePoint lastTargetChange_ {};
};

} // namespace jami::video
