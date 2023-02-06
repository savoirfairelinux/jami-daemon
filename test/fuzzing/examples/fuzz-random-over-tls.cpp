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

static gnutls_session_t captured_session = nullptr;

static std::mutex worker_lock {};
static std::condition_variable cv {};

__attribute__((constructor))
static void
init(void)
{
        std::thread([&] {

                std::unique_lock<std::mutex> lock(worker_lock);

                cv.wait(lock);


                size_t max_size =  gnutls_record_get_max_size(captured_session);
                void *payload = NULL;

                while (true) {

                    size_t size = (size_t)rand() % max_size;

                    payload = realloc(payload, size);

                    printf("Spamming random payload of %zu bytes...\n",
                           size);

                    gnutls_record_send(captured_session, payload, size);

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
