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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <yaml-cpp/yaml.h>

#include "common.h"

/* Jami */
#include "account_const.h"
#include "jami.h"
#include "fileutils.h"
#include "manager.h"

/* Make GCC quiet about unused functions */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

void
wait_for_announcement_of(const std::vector<std::string> accountIDs,
                         std::chrono::seconds timeout)
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::vector<std::atomic_bool> accountsReady(accountIDs.size());

    size_t to_be_announced = accountIDs.size();

    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
            [&,
             accountIDs = std::move(accountIDs)](const std::string& accountID,
                                                 const std::map<std::string, std::string>& details) {
                for (size_t i = 0; i < accountIDs.size(); ++i) {
                    if (accountIDs[i] != accountID) {
                        continue;
                    }

                    try {
                        if ("true"
                            != details.at(libjami::Account::VolatileProperties::DEVICE_ANNOUNCED)) {
                            continue;
                        }
                    } catch (const std::out_of_range&) {
                        continue;
                    }

                    accountsReady[i] = true;
                    cv.notify_one();
                }
            }));

    JAMI_DBG("Waiting for %zu account to be announced...", to_be_announced);

    libjami::registerSignalHandlers(confHandlers);

    CPPUNIT_ASSERT(cv.wait_for(lk, timeout, [&] {
        for (const auto& rdy : accountsReady) {
            if (not rdy) {
                return false;
            }
        }

        return true;
    }));

    libjami::unregisterSignalHandlers();

    JAMI_DBG("%zu account announced!", to_be_announced);
}

void
wait_for_announcement_of(const std::string& accountId,
                         std::chrono::seconds timeout)
{
    wait_for_announcement_of(std::vector<std::string> {accountId}, timeout);
}

void
wait_for_removal_of(const std::vector<std::string> accounts,
                    std::chrono::seconds timeout)
{
    JAMI_INFO("Removing %zu accounts...", accounts.size());

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::atomic_bool accountsRemoved {false};

    size_t current = jami::Manager::instance().getAccountList().size();

    /* Prevent overflow */
    CPPUNIT_ASSERT(current >= accounts.size());

    size_t target = current - accounts.size();

    confHandlers.insert(
        libjami::exportable_callback<libjami::ConfigurationSignal::AccountsChanged>([&]() {
            if (jami::Manager::instance().getAccountList().size() <= target) {
                accountsRemoved = true;
                cv.notify_one();
            }
        }));

    libjami::unregisterSignalHandlers();
    libjami::registerSignalHandlers(confHandlers);

    for (const auto& account : accounts) {
        jami::Manager::instance().removeAccount(account, true);
    }

    CPPUNIT_ASSERT(cv.wait_for(lk, timeout, [&] { return accountsRemoved.load(); }));

    libjami::unregisterSignalHandlers();
}

void
wait_for_removal_of(const std::string& account,
                    std::chrono::seconds timeout)
{
    wait_for_removal_of(std::vector<std::string>{account}, timeout);
}

std::map<std::string, std::string>
load_actors(const std::filesystem::path& from_yaml)
{
    std::map<std::string, std::string> actors {};
    std::map<std::string, std::string> default_details = libjami::getAccountTemplate("RING");

    std::ifstream file(from_yaml);

    CPPUNIT_ASSERT(file.is_open());

    YAML::Node node = YAML::Load(file);

    CPPUNIT_ASSERT(node.IsMap());

    auto default_account = node["default-account"];

    if (default_account.IsMap()) {
        for (const auto& kv : default_account) {
            default_details["Account." + kv.first.as<std::string>()] = kv.second.as<std::string>();
        }
    }

    auto accounts = node["accounts"];

    CPPUNIT_ASSERT(accounts.IsMap());

    for (const auto& kv : accounts) {
        auto account_name = kv.first.as<std::string>();
        auto account = kv.second.as<YAML::Node>();
        auto details = std::map<std::string, std::string>(default_details);

        for (const auto& detail : account) {
            details["Account." + detail.first.as<std::string>()] = detail.second.as<std::string>();
        }

        actors[account_name] = jami::Manager::instance().addAccount(details);
    }

    return actors;
}

std::map<std::string, std::string>
load_actors_and_wait_for_announcement(const std::string& from_yaml)
{
    auto actors = load_actors(from_yaml);

    std::vector<std::string> wait_for;

    wait_for.reserve(actors.size());

    for (auto it = actors.cbegin(); it != actors.cend(); ++it) {
        wait_for.emplace_back(it->second);
    }

    wait_for_announcement_of(wait_for);

    return actors;
}

#pragma GCC diagnostic pop
