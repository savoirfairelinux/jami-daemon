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

namespace jami {

/**
 * Millisecond-resolution timestamp shared by the structures synchronized
 * between devices (ConvInfo, ConversationRequest, Contact, TrustRequest, ...).
 * Serialized with dual keys for backward compatibility: legacy keys carry
 * seconds (read by older devices), "*Ms" keys carry milliseconds.
 */
using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

inline TimePoint
nowMs()
{
    return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
}

constexpr TimePoint
timePointFromSeconds(int64_t seconds)
{
    return TimePoint(std::chrono::seconds(seconds));
}

constexpr TimePoint
timePointFromMilliseconds(int64_t milliseconds)
{
    return TimePoint(std::chrono::milliseconds(milliseconds));
}

constexpr int64_t
toSecondsSinceEpoch(const TimePoint& t)
{
    return std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
}

constexpr int64_t
toMillisecondsSinceEpoch(const TimePoint& t)
{
    return t.time_since_epoch().count();
}

} // namespace jami
