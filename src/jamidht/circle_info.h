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

#include <msgpack.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace jami {

// An account's circle, saved and synced between its devices. Times are seconds since epoch.
struct CircleInfo
{
    // Joined channels and channels left since, by time, so that devices agree on the latest choice.
    std::map<std::string, int64_t> channels;
    std::map<std::string, int64_t> leftChannels;
    // When the local member left; zero while a member.
    int64_t departed {0};
    // The head holding the leave, served until a peer fetched it.
    std::string leaveHead;

    MSGPACK_DEFINE_MAP(channels, leftChannels, departed, leaveHead)
};

// An invitation to a circle, to clone from the inviting device.
struct CircleRequest
{
    std::string from;
    std::string device;
    int64_t received {0};
    int64_t declined {0};
    std::string name;

    MSGPACK_DEFINE_MAP(from, device, received, declined, name)
};

} // namespace jami
