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

#pragma once

#include <yaml-cpp/yaml.h>

/* Jami */
#include "fileutils.h"
#include "manager.h"

static void
wait_for_announcement_of(const std::vector<std::string> accountIDs,
                         std::chrono::seconds timeout = std::chrono::seconds(30))
{
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::vector<std::atomic_bool> accountsReady(accountIDs.size());

    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string& accountID, const std::map<std::string, std::string>& details) {
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

    JAMI_DBG("Waiting for %zu account to be announced...", accountIDs.size());

    DRing::registerSignalHandlers(confHandlers);

    CPPUNIT_ASSERT(cv.wait_for(lk, timeout, [&] {
        for (const auto& rdy : accountsReady) {
            if (not rdy) {
                return false;
            }
        }

        return true;
    }));

    DRing::unregisterSignalHandlers();

    JAMI_DBG("%zu account announced!", accountIDs.size());
}

static void
wait_for_announcement_of(const std::string& accountID,
                         std::chrono::seconds timeout = std::chrono::seconds(30))
{
    wait_for_announcement_of({accountID}, timeout);
}

static std::map<std::string, std::string>
load_actors(const std::string& from_yaml)
{
    std::map<std::string, std::string> actors {};
    std::map<std::string, std::string> default_details = DRing::getAccountTemplate("RING");

    std::ifstream file = jami::fileutils::ifstream(from_yaml);

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

static std::map<std::string, std::string>
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
