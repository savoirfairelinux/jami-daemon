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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <thread>

#include "lib/gnutls.h"

/* Jami */
#include "connectivity/multiplexed_socket.h"

static gnutls_session_t captured_session = nullptr;

static std::mutex worker_lock {};
static std::condition_variable cv;

struct VersionMsg
{
    int v;
    MSGPACK_DEFINE_MAP(v)
};

__attribute__((constructor))
static void
init(void)
{
        std::thread([&] {

                msgpack::sbuffer buffer(8);
                {
                    msgpack::packer<msgpack::sbuffer> pk(&buffer);
                    pk.pack(VersionMsg {rand()});
                }

                msgpack::sbuffer buffer2(16 + buffer.size());
                {
                        msgpack::packer<msgpack::sbuffer> pk(&buffer2);
                        pk.pack_array(2);
                        pk.pack(jami::PROTOCOL_CHANNEL);
                        pk.pack_bin(buffer2.size());
                        pk.pack_bin_body(buffer2.data(), buffer2.size());
                }

                std::unique_lock<std::mutex> lock(worker_lock);

                cv.wait(lock);

                while (true) {
                    gnutls_record_send(captured_session, buffer2.data(), buffer2.size());
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }

        }).detach();
}

void
post_gnutls_init_hook(const gnutls_session_t session)
{
    if (nullptr == captured_session) {
            captured_session = session;
            cv.notify_one();
    }
}
