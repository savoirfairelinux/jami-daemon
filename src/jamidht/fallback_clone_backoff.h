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

#include <algorithm>
#include <chrono>
#include <optional>

namespace jami {

class FallbackCloneBackoff
{
public:
    std::optional<std::chrono::seconds> schedule() noexcept
    {
        if (scheduled_)
            return std::nullopt;
        scheduled_ = true;
        const auto delay = delay_;
        delay_ = std::min(delay_ * 2, MAX_DELAY);
        return delay;
    }

    void timerFired() noexcept { scheduled_ = false; }
    void cancel() noexcept
    {
        delay_ = INITIAL_DELAY;
        scheduled_ = false;
    }

private:
    static constexpr std::chrono::seconds INITIAL_DELAY {5};
    static constexpr std::chrono::seconds MAX_DELAY {12 * 3600};

    std::chrono::seconds delay_ {INITIAL_DELAY};
    bool scheduled_ {false};
};

} // namespace jami
