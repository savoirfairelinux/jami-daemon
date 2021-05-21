/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion>@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include <cstdlib>
#include <cinttypes>

#include "lib/gnutls.h"
#include "lib/supervisor.h"

static FILE* redirect_to = nullptr;

static void
open_redirection(void)
{
    const char* supervisor_says = std::getenv(supervisor::env::tls_out);

    if (nullptr == supervisor_says) {
        redirect_to = stderr;
    } else {
        redirect_to = fopen(supervisor_says, "w");
        assert(redirect_to);
    }
}

static void
print_channel_msg(const ChanneledMessage& msg, const char* direction, bool mutated)
{
    if (nullptr == redirect_to) {
        open_redirection();
    }

    fprintf(redirect_to,
            "\n"
            "================================================================================\n"
            "#:direction %s #:mutated %d #:transport TLS #:channel %" PRIu16 "\n"
            "--------------------------------------------------------------------------------\n"
            "%.*s\n"
            "================================================================================\n",
            direction,
            mutated,
            msg.channel,
            static_cast<int>(msg.data.size()),
            msg.data.data());

    fflush(redirect_to);
}

__weak __used void
on_gnutls_record_sent(const ChanneledMessage& msg)
{
    print_channel_msg(msg, "TX", false);
}

__weak __used void
on_gnutls_record_sent_mutated(const ChanneledMessage& msg)
{
    print_channel_msg(msg, "TX", true);
}

__weak __used bool
mutate_gnutls_record_sent(std::vector<uint8_t>& data)
{
    (void) data;

    return false;
}

__weak __used void
on_gnutls_record_recv(const ChanneledMessage& msg)
{
    print_channel_msg(msg, "RX", false);
}

__weak __used void
on_gnutls_record_recv_mutated(const ChanneledMessage& msg)
{
    print_channel_msg(msg, "RX", true);
}

__weak __used bool
mutate_gnutls_record_recv(std::vector<uint8_t>& data)
{
    (void) data;

    return false;
}

__weak bool
mutate_gnutls_record_recv_channel(uint16_t& channel)
{
    (void) channel;

    return false;
}

BEGIN_WRAPPER(
    ssize_t, gnutls_record_send, gnutls_session_t session, const void* data, size_t data_size)
{
    if (data_size > 0) {
        msgpack::unpacker pac {};
        msgpack::object_handle oh;

        pac.reserve_buffer(data_size);

        memcpy(pac.buffer(), data, data_size);

        pac.buffer_consumed(data_size);

        if (pac.next(oh)) {
            auto msg = oh.get().as<ChanneledMessage>();

            on_gnutls_record_sent(msg);

            if (not mutate_gnutls_record_sent(msg.data)) {
                goto no_mut;
            }

            on_gnutls_record_sent_mutated(msg);

            msgpack::sbuffer buf(msg.data.size());
            msgpack::packer<msgpack::sbuffer> pk(&buf);

            pk.pack_array(2);
            pk.pack(msg.channel);
            pk.pack_bin(msg.data.size());
            pk.pack_bin_body((const char*) msg.data.data(), msg.data.size());

            return this_func(session, buf.data(), buf.size());
        }
    }
no_mut:
    return this_func(session, data, data_size);
}
END_WRAPPER();

BEGIN_WRAPPER(ssize_t, gnutls_record_recv, gnutls_session_t session, void* data, size_t data_size)
{
    ssize_t ret = this_func(session, data, data_size);

    if (ret > 0) {
        msgpack::unpacker pac {};
        msgpack::object_handle oh;

        pac.reserve_buffer(ret);

        memcpy(pac.buffer(), data, ret);

        pac.buffer_consumed(ret);

        if (pac.next(oh)) {
            auto msg = oh.get().as<ChanneledMessage>();

            bool mutated = false;

            on_gnutls_record_recv(msg);

            mutated |= mutate_gnutls_record_recv_channel(msg.channel);
            mutated |= mutate_gnutls_record_recv(msg.data);

            if (not mutated) {
                goto no_mut;
            }

            on_gnutls_record_recv_mutated(msg);

            msgpack::sbuffer buf(msg.data.size());
            msgpack::packer<msgpack::sbuffer> pk(&buf);

            pk.pack_array(2);
            pk.pack(msg.channel);
            pk.pack_bin(msg.data.size());
            pk.pack_bin_body((const char*) msg.data.data(), msg.data.size());

            memcpy(data, buf.data(), buf.size());

            /* Respect GNU TLS API! */
            assert(buf.size() <= data_size);

            ret = buf.size();
        }
    }
no_mut:
    return ret;
}
END_WRAPPER();
