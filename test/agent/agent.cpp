/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion@savoirfairelinux.com>
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
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>

/* DRing */
#include "account_const.h"
#include "jami/presencemanager_interface.h"
#include "jami/callmanager_interface.h"
#include "jami/configurationmanager_interface.h"
#include "jami/conversation_interface.h"

/* agent */
#include "agent/agent.h"

using usize = size_t;

#define LOG_AGENT_STATE() AGENT_DBG("In state %s", __FUNCTION__)

void
Agent::searchForPeers(std::vector<std::string>& peers)
{
    LOG_AGENT_STATE();

    for (auto it = peers.begin(); it != peers.end(); ++it) {
        DRing::sendTrustRequest(accountID_, it->c_str());
        DRing::subscribeBuddy(accountID_, it->c_str(), true);
    }
}

bool
Agent::ping(const std::string& conversation)
{
    LOG_AGENT_STATE();

    auto cv = std::make_shared<std::condition_variable>();
    auto pongReceived = std::make_shared<std::atomic_bool>(false);

    std::string alphabet = "0123456789ABCDEF";
    std::string messageSent;

    for (usize i = 0; i < 16; ++i) {
        messageSent.push_back(alphabet[rand() % alphabet.size()]);
    }

    onMessageReceived_.add([=](const std::string& /* accountID */,
                               const std::string& conversationID,
                               std::map<std::string, std::string> message) {

        if ("text/plain" != message.at("type")) {
            return true;
        }

        auto msg = message.at("body");

        if (pongReceived->load()) {
            return false;
        }

        if (conversationID == conversation and message.at("author") != peerID_
            and msg == "PONG:" + messageSent) {
            *pongReceived = true;
            cv->notify_one();
            return false;
        }

        return true;
    });

    AGENT_INFO("Sending ping `%s` to `%s`", messageSent.c_str(), conversation.c_str());

    DRing::sendMessage(accountID_, conversation, messageSent, "");

    /* Waiting for echo */

    std::mutex mutex;
    std::unique_lock<std::mutex> lck(mutex);

    bool ret = std::cv_status::no_timeout == cv->wait_for(lck, std::chrono::seconds(30))
               and pongReceived->load();

    AGENT_INFO("Pong %s", ret ? "received" : "missing");

    return ret;
}

std::string
Agent::someContact() const
{
    auto members = DRing::getConversationMembers(accountID_, someConversation());

    for (const auto& member : members) {
        if (member.at("uri") != peerID_) {
            return member.at("uri");

        }
    }

    return "";
}

std::string
Agent::someConversation() const
{
    if (conversations_.empty()) {
        return "";
    }

    auto it = conversations_.begin();

    std::advance(it, rand() % conversations_.size());

    return *it;
}

bool
Agent::placeCall(const std::string& contact)
{
    LOG_AGENT_STATE();

    auto cv = std::make_shared<std::condition_variable>();

    auto callID = DRing::placeCall(accountID_, contact);
    auto success = std::make_shared<std::atomic<bool>>(false);
    auto over = std::make_shared<std::atomic<bool>>(false);

    if (callID.empty()) {
        return false;
    }

    onCallStateChanged_.add([=](const std::string& call_id, const std::string& state, signed code) {

        AGENT_INFO("[call:%s] In state %s : %d", callID.c_str(), state.c_str(), code);

        if (call_id != callID) {
            return true;
        }

        bool ret = true;

        if ("CURRENT" == state) {
            success->store(true);
        } else if ("OVER" == state) {
            over->store(true);
            ret = false;
        }

        cv->notify_one();

        return ret;
    });

    std::mutex mtx;
    std::unique_lock<std::mutex> lck {mtx};

    AGENT_INFO("Waiting for call %s", callID.c_str());

    /* TODO - Parametize me */
    cv->wait_for(lck, std::chrono::seconds(30), [=]{
        return success->load() or over->load();
    });

    if (success->load()) {
        AGENT_INFO("[call:%s] to %s: SUCCESS", callID.c_str(), contact.c_str());
        DRing::hangUp(callID);
    } else {
        AGENT_INFO("[call:%s] to %s: FAIL", callID.c_str(), contact.c_str());
    }

    if (not over->load()) {
        cv->wait_for(lck, std::chrono::seconds(30), [=] { return over->load(); });
    }

    return success->load();
}

void
Agent::wait(std::chrono::seconds period)
{
    LOG_AGENT_STATE();

    std::this_thread::sleep_for(period);
}

void
Agent::setDetails(const std::map<std::string, std::string>& details)
{
    LOG_AGENT_STATE();

    auto cv = std::make_shared<std::condition_variable>();
    auto cont = std::make_shared<std::atomic<bool>>(true);

    std::string info("Setting details:\n");

    for (const auto& [key, value] : details) {
        info += key + " = " + value + "\n";
    }

    AGENT_INFO("%s", info.c_str());

    DRing::setAccountDetails(accountID_, details);
}

void
Agent::activate(bool state)
{
    LOG_AGENT_STATE();

    DRing::setAccountActive(accountID_, state);

    if (state) {
        waitForAnnouncement();
    }
}

void
Agent::exportToArchive(const std::string& path)
{
    LOG_AGENT_STATE();

    AGENT_ASSERT(DRing::exportToFile(accountID_,
                                     path),
                 "Failed to export account to `%s`", path.c_str());
}

void
Agent::importFromArchive(const std::string& path)
{
    LOG_AGENT_STATE();

    std::map<std::string, std::string> details;

    details[DRing::Account::ConfProperties::TYPE] = "RING";
    details[DRing::Account::ConfProperties::DISPLAYNAME] = "AGENT";
    details[DRing::Account::ConfProperties::ALIAS] = "AGENT";
    details[DRing::Account::ConfProperties::ARCHIVE_PASSWORD] = "";
    details[DRing::Account::ConfProperties::ARCHIVE_PIN] = "";
    details[DRing::Account::ConfProperties::ARCHIVE_PATH] = path;

    AGENT_ASSERT(accountID_ == DRing::addAccount(details, accountID_), "Bad accountID");

    details = DRing::getAccountDetails(accountID_);

    waitForAnnouncement();

    AGENT_ASSERT("AGENT" == details.at(DRing::Account::ConfProperties::DISPLAYNAME),
                 "Bad display name");

    peerID_ = details.at(DRing::Account::ConfProperties::USERNAME);
    conversations_ = DRing::getConversations(accountID_);

    AGENT_INFO("Using account %s - %s", accountID_.c_str(), peerID_.c_str());
}

void
Agent::ensureAccount()
{
    LOG_AGENT_STATE();

    std::map<std::string, std::string> details;

    details = DRing::getAccountDetails(accountID_);

    if (details.empty()) {
        details[DRing::Account::ConfProperties::TYPE] = "RING";
        details[DRing::Account::ConfProperties::DISPLAYNAME] = "AGENT";
        details[DRing::Account::ConfProperties::ALIAS] = "AGENT";
        details[DRing::Account::ConfProperties::ARCHIVE_PASSWORD] = "";
        details[DRing::Account::ConfProperties::ARCHIVE_PIN] = "";
        details[DRing::Account::ConfProperties::ARCHIVE_PATH] = "";

        AGENT_ASSERT(accountID_ == DRing::addAccount(details, accountID_), "Bad accountID");
    }

    waitForAnnouncement();

    details = DRing::getAccountDetails(accountID_);

    AGENT_ASSERT("AGENT" == details.at(DRing::Account::ConfProperties::DISPLAYNAME),
                 "Bad display name");

    peerID_ = details.at(DRing::Account::ConfProperties::USERNAME);
    conversations_ = DRing::getConversations(accountID_);

    AGENT_INFO("Using account %s - %s", accountID_.c_str(), peerID_.c_str());
}


Agent&
Agent::instance()
{
    static Agent agent;

    return agent;
}

void
Agent::installSignalHandlers()
{
    using namespace std::placeholders;
    using std::bind;

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> handlers;

    handlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCallWithMedia>(
        bind(&Agent::Handler<const std::string&,
                             const std::string&,
                             const std::string&,
                             const std::vector<DRing::MediaMap>>::execute,
             &onIncomingCall_,
             _1,
             _2,
             _3,
             _4)));

    handlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        bind(&Agent::Handler<const std::string&, const std::string&, signed>::execute,
             &onCallStateChanged_,
             _1,
             _2,
             _3)));

    handlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        bind(&Agent::Handler<const std::string&,
                             const std::string&,
                             std::map<std::string, std::string>>::execute,
             &onMessageReceived_,
             _1,
             _2,
             _3)));

    handlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            bind(&Agent::Handler<const std::string&,
                                 const std::string&,
                                 std::map<std::string, std::string>>::execute,
                 &onConversationRequestReceived_,
                 _1,
                 _2,
                 _3)));

    handlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        bind(&Agent::Handler<const std::string&, const std::string&>::execute,
             &onConversationReady_,
             _1,
             _2)));

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::ContactAdded>(
        bind(&Agent::Handler<const std::string&, const std::string&, bool>::execute,
             &onContactAdded_,
             _1,
             _2,
             _3)));

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::RegistrationStateChanged>(
        bind(&Agent::Handler<const std::string&, const std::string&, int, const std::string&>::execute,
             &onRegistrationStateChanged_,
             _1,
             _2,
             _3,
             _4)));

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
                        bind(&Agent::Handler<const std::string&, const std::map<std::string, std::string>&>::execute,
                             &onVolatileDetailsChanged_,
                             _1,
                             _2)));

    DRing::registerSignalHandlers(handlers);
}

void
Agent::registerStaticCallbacks()
{
    onIncomingCall_.add([=](const std::string& accountID,
                            const std::string& callID,
                            const std::string& peerDisplayName,
                            const std::vector<DRing::MediaMap> mediaList) {
        (void) accountID;
        (void) peerDisplayName;

        std::string medias("");

        for (const auto& media : mediaList) {
            for (const auto& [key, value] : media) {
                medias += key + "=" + value + "\n";
            }
        }

        AGENT_INFO("Incoming call `%s` from `%s` with medias:\n`%s`",
                   callID.c_str(),
                   peerDisplayName.c_str(),
                   medias.c_str());

        AGENT_ASSERT(DRing::acceptWithMedia(callID, mediaList),
                     "Failed to accept call `%s`",
                     callID.c_str());

        return true;
    });

    onMessageReceived_.add([=](const std::string& accountID,
                               const std::string& conversationID,
                               std::map<std::string, std::string> message) {
        (void) accountID;

        /* Read only text message */
        if ("text/plain" != message.at("type")) {
            return true;
        }

        auto author = message.at("author");

        /* Skip if sent by agent */
        if (peerID_ == author) {
            return true;
        }

        auto msg = message.at("body");

        AGENT_INFO("Incomming message `%s` from %s", msg.c_str(), author.c_str());

        /* Echo back */
        if (0 != msg.rfind("PONG:", 0)) {
            DRing::sendMessage(accountID_, conversationID, "PONG:" + msg, "");
        }

        return true;
    });

    onConversationRequestReceived_.add([=](const std::string& accountID,
                                           const std::string& conversationID,
                                           std::map<std::string, std::string> meta) {
        (void) meta;

        AGENT_INFO("Conversation request received for account %s", accountID.c_str());

        DRing::acceptConversationRequest(accountID, conversationID);

        return true;
    });

    onConversationReady_.add([=](const std::string& accountID, const std::string& conversationID) {
        (void) accountID;
        conversations_.emplace_back(conversationID);
        return true;
    });

    onContactAdded_.add([=](const std::string& accountID, const std::string& URI, bool confirmed) {
        AGENT_INFO("Contact added `%s` : %s", URI.c_str(), confirmed ? "accepted" : "refused");
        if (confirmed) {
            DRing::subscribeBuddy(accountID, URI, true);
        }
        return true;
    });
}

void
Agent::waitForAnnouncement(std::chrono::seconds timeout)
{
    auto cv = std::make_shared<std::condition_variable>();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};

    onVolatileDetailsChanged_.add([=](const std::string& accountID,
                                      const std::map<std::string, std::string>& details) {

                    if (accountID_ != accountID) {
                        return true;
                    }

                    try {
                        if ("true"
                            != details.at(DRing::Account::VolatileProperties::DEVICE_ANNOUNCED)) {
                            return true;
                        }
                    } catch (const std::out_of_range&) {
                        return true;
                    }

                    cv->notify_one();

                    return false;
    });

    AGENT_ASSERT(std::cv_status::no_timeout == cv->wait_for(lk, timeout),
                 "Timeout while waiting for account announcement on DHT");
}

void
Agent::init()
{
    LOG_AGENT_STATE();

    installSignalHandlers();
    registerStaticCallbacks();
}

void
Agent::fini()
{
    LOG_AGENT_STATE();

    DRing::unregisterSignalHandlers();
}
