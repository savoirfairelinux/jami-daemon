/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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

#include <msgpack.hpp>

#include "connectivity/multiplexed_socket.h"

#include "lib/gnutls.h"

enum class ChannelRequestState {
    REQUEST,
    ACCEPT,
    DECLINE,
};

/**
 * That msgpack structure is used to request a new channel (id, name)
 * Transmitted over the TLS socket
 */
struct ChannelRequest
{
    std::string name {};
    uint16_t channel {0};
    ChannelRequestState state {ChannelRequestState::REQUEST};
    MSGPACK_DEFINE(name, channel, state)
};

/*
 * Mangle channel
 */
bool
mutate_gnutls_record_send(ChanneledMessage& msg)
{
    try {
        msgpack::unpacked result;
        msgpack::unpack(result, (const char*) msg.data.data(), msg.data.size(), 0);
        auto object = result.get();
        auto req = object.as<ChannelRequest>();

        int state = rand() % 8;

        static_assert(sizeof(state) == sizeof(req.state));
        memcpy(&req.state, &state, sizeof(state));

        msgpack::sbuffer buffer(512);
        msgpack::pack(buffer, req);

        msg.data.clear();

        for (size_t i = 0; i < buffer.size(); ++i) {
            msg.data.emplace_back(buffer.data()[i]);
        }

    } catch (...) {
        return false;
    }

    return true;
}

MSGPACK_ADD_ENUM(ChannelRequestState);
