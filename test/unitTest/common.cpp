/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <yaml-cpp/yaml.h>

#include "common.h"

/* Jami */
#include "account_const.h"
#include "jami.h"
#include "fileutils.h"
#include "jamidht/jamiaccount.h"
#include "manager.h"

/* Make GCC quiet about unused functions */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

void
add_confirmed_contact(const std::string& accountId, const std::string& contactId)
{
    auto account = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId);
    auto contact = jami::Manager::instance().getAccount<jami::JamiAccount>(contactId);
    CPPUNIT_ASSERT(account);
    CPPUNIT_ASSERT(contact);

    const auto accountWasEnabled = account->isEnabled();
    const auto contactWasEnabled = contact->isEnabled();
    std::string accountUri;
    std::string contactUri;
    auto isConfirmed = [](const auto& actor, const auto& uri) {
        auto details = actor->getContactInfo(uri);
        return details && details->isActive() && details->confirmed;
    };
    auto isAnnounced = [](const auto& actor) {
        auto details = actor->getVolatileAccountDetails();
        auto announced = details.find(libjami::Account::VolatileProperties::DEVICE_ANNOUNCED);
        return announced != details.end() && announced->second == "true";
    };
    auto isStopped = [](const auto& actor) {
        auto details = actor->getVolatileAccountDetails();
        auto status = details.find(libjami::Account::ConfProperties::Registration::STATUS);
        return status != details.end() && status->second == "UNREGISTERED";
    };

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers;
    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    bool requestReceived = false;

    handlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
        [&](const std::string& id, const std::map<std::string, std::string>&) {
            std::lock_guard lock {mtx};
            if (id == accountId || id == contactId)
                cv.notify_one();
        }));
    handlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
        [&](const std::string& id, const std::string&, const std::string& from, const std::vector<uint8_t>&, time_t) {
            std::lock_guard lock {mtx};
            if (id == contactId && from == accountUri) {
                requestReceived = true;
                cv.notify_one();
            }
        }));
    handlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ContactAdded>(
        [&](const std::string& id, const std::string&, bool) {
            std::lock_guard lock {mtx};
            if (id == accountId || id == contactId)
                cv.notify_one();
        }));
    handlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& id, const std::string&) {
            std::lock_guard lock {mtx};
            if (id == accountId || id == contactId)
                cv.notify_one();
        }));
    libjami::unregisterSignalHandlers();
    libjami::registerSignalHandlers(handlers);

    lk.unlock();
    if (!accountWasEnabled)
        jami::Manager::instance().sendRegister(accountId, true);
    if (!contactWasEnabled)
        jami::Manager::instance().sendRegister(contactId, true);
    lk.lock();
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(WAIT_FOR_ANNOUNCEMENT_TIMEOUT), [&] {
        return isAnnounced(account) && isAnnounced(contact);
    }));

    accountUri = account->getUsername();
    contactUri = contact->getUsername();
    CPPUNIT_ASSERT(!accountUri.empty());
    CPPUNIT_ASSERT(!contactUri.empty());
    const auto contactAlreadyConfirmed = isConfirmed(account, contactUri);
    const auto accountAlreadyConfirmed = isConfirmed(contact, accountUri);
    const auto accountConversation = account->convModule()->getOneToOneConversation(contactUri);
    const auto contactConversation = contact->convModule()->getOneToOneConversation(accountUri);
    lk.unlock();

    if (!(contactAlreadyConfirmed && accountAlreadyConfirmed && !accountConversation.empty()
          && !contactConversation.empty())) {
        account->addContact(contactUri);
        account->sendTrustRequest(contactUri, {});
        lk.lock();
        CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(WAIT_FOR_ANNOUNCEMENT_TIMEOUT), [&] {
            return requestReceived || isConfirmed(contact, accountUri);
        }));
        lk.unlock();
        if (!isConfirmed(contact, accountUri))
            CPPUNIT_ASSERT(contact->acceptTrustRequest(accountUri));
        lk.lock();
        const auto isContactReady = [&] {
            const auto accountConv = account->convModule()->getOneToOneConversation(contactUri);
            const auto contactConv = contact->convModule()->getOneToOneConversation(accountUri);
            return isConfirmed(account, contactUri) && isConfirmed(contact, accountUri) && !accountConv.empty()
                   && accountConv == contactConv;
        };
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(WAIT_FOR_ANNOUNCEMENT_TIMEOUT);
        while (!isContactReady() && std::chrono::steady_clock::now() < deadline)
            cv.wait_for(lk, std::chrono::milliseconds(100));
        CPPUNIT_ASSERT(isContactReady());
        lk.unlock();
    }

    if (!accountWasEnabled)
        jami::Manager::instance().sendRegister(accountId, false);
    if (!contactWasEnabled)
        jami::Manager::instance().sendRegister(contactId, false);
    lk.lock();
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(WAIT_FOR_ANNOUNCEMENT_TIMEOUT), [&] {
        return (accountWasEnabled || isStopped(account)) && (contactWasEnabled || isStopped(contact));
    }));

    lk.unlock();
    libjami::unregisterSignalHandlers();
}

void
wait_for_announcement_of(const std::vector<std::string> accountIDs, std::chrono::seconds timeout)
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::vector<std::atomic_bool> accountsReady(accountIDs.size());

    size_t to_be_announced = accountIDs.size();

    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
        [&, accountIDs = std::move(accountIDs)](const std::string& accountID,
                                                const std::map<std::string, std::string>& details) {
            for (size_t i = 0; i < accountIDs.size(); ++i) {
                if (accountIDs[i] != accountID) {
                    continue;
                }

                if (jami::Manager::instance().getAccount(accountID)->getAccountType() == "SIP") {
                    auto daemonStatus = details.at(libjami::Account::ConfProperties::Registration::STATUS);
                    if (daemonStatus != "REGISTERED") {
                        continue;
                    }
                } else {
                    try {
                        if ("true" != details.at(libjami::Account::VolatileProperties::DEVICE_ANNOUNCED)) {
                            continue;
                        }
                    } catch (const std::out_of_range&) {
                        continue;
                    }
                }

                accountsReady[i] = true;
                cv.notify_one();
            }
        }));

    JAMI_LOG("Waiting for {} account to be announced...", to_be_announced);

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

    JAMI_LOG("{} account announced!", to_be_announced);
}

void
wait_for_announcement_of(const std::string& accountId, std::chrono::seconds timeout)
{
    wait_for_announcement_of(std::vector<std::string> {accountId}, timeout);
}

void
wait_for_removal_of(const std::vector<std::string> accounts, std::chrono::seconds timeout)
{
    JAMI_LOG("Removing {} accounts...", accounts.size());

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::atomic_bool accountsRemoved {false};

    size_t current = jami::Manager::instance().getAccountList().size();

    /* Prevent overflow */
    CPPUNIT_ASSERT(current >= accounts.size());

    size_t target = current - accounts.size();

    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::AccountsChanged>([&]() {
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
wait_for_removal_of(const std::string& account, std::chrono::seconds timeout)
{
    wait_for_removal_of(std::vector<std::string> {account}, timeout);
}

std::map<std::string, std::string>
load_actors(const std::filesystem::path& from_yaml)
{
    std::map<std::string, std::string> actors {};

    std::ifstream file(from_yaml);

    CPPUNIT_ASSERT(file.is_open());

    YAML::Node node = YAML::Load(file);

    CPPUNIT_ASSERT(node.IsMap());

    auto default_account = node["default-account"];

    std::map<std::string, std::string> default_details = libjami::getAccountTemplate(
        default_account["type"].as<std::string>());
    if (default_account.IsMap()) {
        for (const auto& kv : default_account) {
            auto key = kv.first.as<std::string>();
            if (default_details.find(key) != default_details.end()) {
                default_details[key] = kv.second.as<std::string>();
            } else {
                default_details["Account." + key] = kv.second.as<std::string>();
            }
        }
    }

    auto accounts = node["accounts"];

    CPPUNIT_ASSERT(accounts.IsMap());

    for (const auto& kv : accounts) {
        auto account_name = kv.first.as<std::string>();
        auto account = kv.second.as<YAML::Node>();
        auto details = std::map<std::string, std::string>(default_details);

        for (const auto& detail : account) {
            auto key = detail.first.as<std::string>();
            if (details.find(key) != details.end()) {
                details[key] = detail.second.as<std::string>();
            } else {
                details["Account." + key] = detail.second.as<std::string>();
            }
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
