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

#ifdef WIP
#include <opendht/dhtrunner.h>
#include <opendht/thread_pool.h>
#include <opendht/default_types.h>

namespace dht {

__weak
bool mutate_dht_encrypted_ImMessage(ImMessage& msg)
{
        printf("message!\n");

        return false;
}

/* TODO - Is there a way to not tod without the mangled name?  Use nm(1) for that.  */
BEGIN_METHOD_WRAPPER(
        _ZN3dht9DhtRunner12putEncryptedENS_4HashILm20EEES2_St10shared_ptrINS_5ValueEESt8functionIFvbRKSt6vectorIS3_INS_4NodeEESaIS9_EEEEb,
        void, DhtRunner::putEncrypted, InfoHash hash, InfoHash to, std::shared_ptr<Value> value, DoneCallback cb, bool permanent)
{
        if (value->type == ImMessage::TYPE.id) {
                ImMessage uv = Value::unpack<ImMessage>(value);
                bool mutated = mutate_dht_encrypted_ImMessage(uv);
        }

        this_func(this, hash, to, value, cb, permanent);

}
END_WRAPPER();

};
#endif
