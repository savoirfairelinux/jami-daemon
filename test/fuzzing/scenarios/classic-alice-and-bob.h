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

#include <condition_variable>

#include "manager.h"
#include "connectivity/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "jami.h"
#include "account_const.h"

#include "lib/utils.h"

int main(void)
{
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));

        if (not jami::Manager::instance().initialized) {
            assert(libjami::start("dring-sample.yml"));
        }

        auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");

        auto alice = actors["alice"];
        auto bob   = actors["bob"];

        auto aliceAccount = jami::Manager::instance().getAccount<jami::JamiAccount>(alice);
        auto bobAccount   = jami::Manager::instance().getAccount<jami::JamiAccount>(bob);

        auto aliceUri = aliceAccount->getUsername();
        auto bobUri   = bobAccount->getUsername();

        std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
        std::atomic_bool callReceived {false};
        std::mutex mtx;
        std::unique_lock<std::mutex> lk {mtx};
        std::condition_variable cv;

        confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCall>(
                                    [&](const std::string&, const std::string&, const std::string&) {
                                            callReceived = true;
                                            cv.notify_one();
                                    }));

        libjami::registerSignalHandlers(confHandlers);

        auto call = aliceAccount->newOutgoingCall(bobUri);

        assert(cv.wait_for(lk, std::chrono::seconds(30), [&] { return callReceived.load(); }));

        std::this_thread::sleep_for(std::chrono::seconds(60));

        wait_for_removal_of({alice, bob});

        libjami::fini();

        return 0;
}
