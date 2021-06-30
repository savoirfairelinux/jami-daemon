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

/* std */
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <cassert>
#include <vector>

/* DRing */
#include "dring.h"
#include "dring/account_const.h"
#include "dring/configurationmanager_interface.h"
#include "dring/call_const.h"

/* agent */
#include "agent/utils.h"

void
wait_for_announcement_of(const std::vector<std::string> accountIDs, std::chrono::seconds timeout)
{
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::vector<std::atomic_bool> accountsReady(accountIDs.size());

    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&,
             accountIDs = std::move(accountIDs)](const std::string& accountID,
                                                 const std::map<std::string, std::string>& details) {
                for (size_t i = 0; i < accountIDs.size(); ++i) {
                    if (accountIDs[i] != accountID) {
                        continue;
                    }

                    try {
                        if ("true"
                            != details.at(DRing::Account::VolatileProperties::DEVICE_ANNOUNCED)) {
                            continue;
                        }
                    } catch (const std::out_of_range&) {
                        continue;
                    }

                    accountsReady[i] = true;
                    cv.notify_one();
                }
            }));

    DRing::registerSignalHandlers(confHandlers);

    assert(cv.wait_for(lk, timeout, [&] {
        for (const auto& rdy : accountsReady) {
            if (not rdy) {
                return false;
            }
        }

        return true;
    }));

    DRing::unregisterSignalHandlers();
}

void
wait_for_announcement_of(const std::string& accountId, std::chrono::seconds timeout)
{
    wait_for_announcement_of(std::vector<std::string> {accountId}, timeout);
}
