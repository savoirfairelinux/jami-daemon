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

#include "lib/gnutls.h"
#include "lib/syslog.h"

/*
 * Reverse channel and data in packed message
 */
void
pack_gnutls_record_recv(msgpack::sbuffer& buf, const ChanneledMessage& msg)
{
    msgpack::packer<msgpack::sbuffer> pk(&buf);
#if 0
     pk.pack_array(2);
     pk.pack_bin(msg.data.size());
     pk.pack_bin_body((const char*) msg.data.data(), msg.data.size());
     pk.pack(msg.channel);
#else
    pk.pack_array(1);
    pk.pack(msg.channel);
#endif
}

bool
mutate_gnutls_record_recv(ChanneledMessage& msg)
{
    (void) msg;

    return true;
}
