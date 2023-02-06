/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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

static std::atomic<size_t> packet_ID = 0;

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
print_channel_msg(const ChanneledMessage& msg, const char* direction, bool mutated, size_t packed_id)
{
    if (nullptr == redirect_to) {
        open_redirection();
    }

    fprintf(redirect_to,
            "\n"
            "================================================================================\n"
            "#:direction %s #:mut %d #:transport TLS #:channel %" PRIu16 " #:ID %" PRIu64   "\n"
            "--------------------------------------------------------------------------------\n"
            "%.*s\n"
            "================================================================================\n",
            direction,
            mutated,
            msg.channel,
            packed_id,
            static_cast<int>(msg.data.size()),
            msg.data.data());

    fflush(redirect_to);
}

#if 0
static std::map<std::string;

static void
register_channel(const std::string& name, uint16_t channel)
{

}
#endif

__weak
bool
mutate_gnutls_record_send(ChanneledMessage& msg)
{
    (void) msg;

    return false;
}

__weak
bool
mutate_gnutls_record_recv(ChanneledMessage& msg)
{
    (void) msg;

    return false;
}

__weak
void
pack_gnutls_record_recv(msgpack::sbuffer& buf, const ChanneledMessage& msg)
{
    msgpack::packer<msgpack::sbuffer> pk(&buf);

    pk.pack_array(2);
    pk.pack(msg.channel);
    pk.pack_bin(msg.data.size());
    pk.pack_bin_body((const char*) msg.data.data(), msg.data.size());
}

__weak
void
pack_gnutls_record_send(msgpack::sbuffer& buf, const ChanneledMessage& msg)
{
    msgpack::packer<msgpack::sbuffer> pk(&buf);

    pk.pack_array(2);
    pk.pack(msg.channel);
    pk.pack_bin(msg.data.size());
    pk.pack_bin_body((const char*) msg.data.data(), msg.data.size());
}

BEGIN_WRAPPER(ssize_t, gnutls_record_send, gnutls_session_t session, const void* data, size_t data_size)
{
    if (data_size > 0) {

        msgpack::unpacker pac {};
        msgpack::object_handle oh;

        pac.reserve_buffer(data_size);

        memcpy(pac.buffer(), data, data_size);

        pac.buffer_consumed(data_size);

        if (pac.next(oh)) {

                try {
                        auto msg = oh.get().as<ChanneledMessage>();

                        size_t ID = packet_ID++;

                        bool mutated;

                        print_channel_msg(msg, "TX", false, ID);

                        mutated = mutate_gnutls_record_send(msg);

                        if (not mutated) {
                                goto no_mut;

                        }

                        print_channel_msg(msg, "TX", true, ID);

                        msgpack::sbuffer buf(16 +msg.data.size());

                        pack_gnutls_record_send(buf, msg);

                        return this_func(session, buf.data(), buf.size());

                } catch (...) { }
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

            size_t ID = packet_ID++;

            bool mutated;
#if 0
            if (CONTROL_CHANNEL == msg.channel) {

                    try {
                            msgpack::unpacked result;
                            msgpack::unpack(result, (const char*) msg.data.data(), msg.data.size(), 0);
                            auto obj = result.get();

                            auto req = obj.as<ChannelRequest>();

                            if (ChannelRequestState::ACCEPT == req.state) {
                                    register_channel(req.name, req.channel);
                            }

                    } catch (...) {

                    }
            }
#endif
            print_channel_msg(msg, "RX", false, ID);

            mutated = mutate_gnutls_record_recv(msg);

            if (not mutated) {
                goto no_mut;
            }

            print_channel_msg(msg, "RX", true, ID);

            msgpack::sbuffer buf(16 + msg.data.size());

            pack_gnutls_record_recv(buf, msg);

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

__weak
void
post_gnutls_init_hook(gnutls_session_t session)
{
    (void)session;
}

__weak
void
pre_gnutls_deinit_hook(gnutls_session_t session)
{
    (void)session;
}

BEGIN_WRAPPER(int, gnutls_init, gnutls_session_t * session, unsigned int flags)
{
        int ret;

        ret = this_func(session, flags);

        post_gnutls_init_hook(*session);

        return ret;
}
END_WRAPPER();

BEGIN_WRAPPER(ssize_t, gnutls_record_recv_seq, gnutls_session_t session, void * data, size_t data_size, unsigned char * seq)
{
    ssize_t ret = this_func(session, data, data_size, seq);

    if (ret > 0) {

        msgpack::unpacker pac {};
        msgpack::object_handle oh;

        pac.reserve_buffer(ret);

        memcpy(pac.buffer(), data, ret);

        pac.buffer_consumed(ret);

        if (pac.next(oh)) {

            auto msg = oh.get().as<ChanneledMessage>();

            size_t ID = packet_ID++;

            bool mutated;

            print_channel_msg(msg, "RX", false, ID);

            mutated = mutate_gnutls_record_recv(msg);

            if (not mutated) {
                goto no_mut;
            }

            print_channel_msg(msg, "RX", true, ID);

            msgpack::sbuffer buf(16 + msg.data.size());

            pack_gnutls_record_recv(buf, msg);

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
