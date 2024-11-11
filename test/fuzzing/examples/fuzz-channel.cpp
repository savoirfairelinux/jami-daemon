/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

#include <cstdlib>
#include <set>

#include "connectivity/multiplexed_socket.h"

#include "lib/gnutls.h"

#if 0
static std::set<uint16_t> known_channel {};
static std::vector<channel

__attribute__((constructor))
static void channel_spammer(void)
{
        std::thread([&]{

                std::unique_lock ulock(spammer_lock);
                while (true) {

                }

        }).detach();
}


/*
 * Mangle channel
 */
bool
mutate_gnutls_record_recv(ChanneledMessage& msg)
{
    known_channel.emplace(msg.channel);

    auto it = known_channel.begin();

    std::advance(it, rand() % known_channel.size());

    msg.channel = *it;

    return true;
}
#endif
