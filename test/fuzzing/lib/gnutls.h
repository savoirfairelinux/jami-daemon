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

#pragma once

#include <gnutls/gnutls.h>
#include <gnutls/dtls.h>

#include <msgpack.hpp>

/* TODO - Import this from jami */
struct ChanneledMessage
{
    uint16_t channel;
    std::vector<uint8_t> data;
    MSGPACK_DEFINE(channel, data)
};

extern "C" {

extern void inject_into_gnutls(const ChanneledMessage& msg);

extern void post_gnutls_init_hook(gnutls_session_t session);
extern void pre_gnutls_deinit_hook(gnutls_session_t session);

extern void pre_mutate_gnutls_record_send_hook(const ChanneledMessage& msg);
extern void post_mutate_gnutls_record_send_hook(const ChanneledMessage& msg, bool hasMutated);
extern bool mutate_gnutls_record_send(ChanneledMessage& msg);

extern void pre_mutate_gnutls_record_recv_hook(const ChanneledMessage& msg);
extern void post_mutate_gnutls_record_recv_hook(const ChanneledMessage& msg, bool hasMutated);
extern bool mutate_gnutls_record_recv(ChanneledMessage& msg);

};
