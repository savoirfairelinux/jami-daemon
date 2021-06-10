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
#include <set>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "manager.h"
#include "jamidht/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "dring.h"
#include "account_const.h"

#include "lib/gnutls.h"
#include "lib/utils.h"

static std::thread spammer;
static gnutls_session_t spamming_session = nullptr;

void post_gnutls_init_hook(gnutls_session_t session)
{
        if (not session) {
                return;
        }

        if (spamming_session) {
                return;
        }

        printf("Starting channel spammer...\n");

        spamming_session = session;

        spammer = std::thread([&, session=session]{

                std::this_thread::sleep_for(std::chrono::seconds(5));

                printf("Starting spamming!\n");

                jami::ChannelRequest val;

                val.name  = "sip";
                val.state = jami::ChannelRequestState::REQUEST;

                for (size_t i=0; i<UINT16_MAX; ++i) {

                        if (not spamming_session) {
                                break;
                        }

                        val.channel = i;

                        msgpack::sbuffer buffer1(256);
                        msgpack::pack(buffer1, val);

                        msgpack::sbuffer buffer2(16 + buffer1.size());
                        msgpack::packer<msgpack::sbuffer> pk(&buffer2);

                        pk.pack_array(2);
                        pk.pack(jami::CONTROL_CHANNEL);
                        pk.pack_bin(buffer1.size());
                        pk.pack_bin_body((const char*) buffer1.data(), buffer1.size());

                        gnutls_record_send(session, buffer2.data(), buffer2.size());
                        std::this_thread::sleep_for(std::chrono::microseconds(1000));
                }

                printf("Stopping spamming!\n");

        });
}

void pre_gnutls_deinit_hook(gnutls_session_t session)
{
        if (session and session == spamming_session) {
                spamming_session = nullptr;
                spammer.join();
                printf("Channel spammer killed!\n");
        }
}


int main(void)
{
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));

        if (not jami::Manager::instance().initialized) {
            assert(DRing::start("dring-sample.yml"));
        }

        auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");

        auto alice = actors["alice"];
        auto bob   = actors["bob"];

        auto aliceAccount = jami::Manager::instance().getAccount<jami::JamiAccount>(alice);
        auto bobAccount   = jami::Manager::instance().getAccount<jami::JamiAccount>(bob);

        auto aliceUri = aliceAccount->getUsername();
        auto bobUri   = bobAccount->getUsername();

        std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
        std::atomic_bool callReceived {false};
        std::mutex mtx;
        std::unique_lock<std::mutex> lk {mtx};
        std::condition_variable cv;

        confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCall>(
                                    [&](const std::string&, const std::string&, const std::string&) {
                                            callReceived = true;
                                            cv.notify_one();
                                    }));

        DRing::registerSignalHandlers(confHandlers);

        auto call = aliceAccount->newOutgoingCall(bobUri);

        assert(cv.wait_for(lk, std::chrono::seconds(30), [&] { return callReceived.load(); }));

        std::this_thread::sleep_for(std::chrono::seconds(60));

        wait_for_removal_of({alice, bob});

        DRing::fini();

        return 0;
}
