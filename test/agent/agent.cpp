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

/* Third parties */
#include <yaml-cpp/yaml.h>

/* DRing */
#include "account_const.h"
#include "dring/presencemanager_interface.h"
#include "dring/callmanager_interface.h"
#include "dring/configurationmanager_interface.h"
#include "dring/conversation_interface.h"

/* agent */
#include "agent/agent.h"
#include "agent/bt.h"
#include "agent/utils.h"

using usize = size_t;
using u32 = uint32_t;

#define LOG_AGENT_STATE()        JAMI_INFO("AGENT: In state %s", __FUNCTION__)
#define AGENT_ERR(FMT, ARGS...)  JAMI_ERR("AGENT: " FMT, ##ARGS)
#define AGENT_INFO(FMT, ARGS...) JAMI_INFO("AGENT: " FMT, ##ARGS)
#define AGENT_DBG(FMT, ARGS...)  JAMI_DBG("AGENT: " FMT, ##ARGS)
#define AGENT_ASSERT(COND, MSG, ARGS...) \
    if (not(COND)) { \
        AGENT_ERR(MSG, ##ARGS); \
        exit(1); \
    }

void
Agent::initBehavior()
{
    using std::bind;

    BT::register_behavior("search-peer", bind(&Agent::searchPeer, this));
    BT::register_behavior("wait", bind(&Agent::wait, this));
    BT::register_behavior("echo", bind(&Agent::echo, this));
    BT::register_behavior("make-call", bind(&Agent::makeCall, this));
    BT::register_behavior("true", bind(&Agent::True, this));
    BT::register_behavior("false", bind(&Agent::False, this));
}

void
Agent::configure(const std::string& yaml_config)
{
    std::ifstream file = std::ifstream(yaml_config);

    AGENT_ASSERT(file.is_open(), "Failed to open configuration file `%s`", yaml_config.c_str());

    YAML::Node node = YAML::Load(file);

    auto peers = node["peers"];

    AGENT_ASSERT(peers.IsSequence(), "Configuration node `peers` must be a sequence");

    for (const auto& peer : peers) {
        peers_.emplace_back(peer.as<std::string>());
    }

    root_ = BT::from_yaml(node["behavior"]);

    /* params */
    auto params = node["params"];

    if (params) {
        auto accountID_details = DRing::getAccountDetails(accountID_);
        assert(params.IsSequence());
        for (const auto& param : params) {
            assert(param.IsSequence());
            for (const auto& details : param) {
                assert(details.IsMap());
                for (const auto& detail : details) {
                    auto first = detail.first.as<std::string>();
                    auto second = detail.second.as<std::string>();
                    accountID_details["Account." + first] = second;
                }
            }
            params_.emplace_back([this, accountID_details = std::move(accountID_details)] {
                DRing::setAccountDetails(accountID_, accountID_details);
            });
        }
    }
}

void
Agent::ensureAccount()
{
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

        wait_for_announcement_of(accountID_);

        details = DRing::getAccountDetails(accountID_);
    }

    AGENT_ASSERT("AGENT" == details.at(DRing::Account::ConfProperties::DISPLAYNAME),
                 "Bad display name");

    peerID_ = details.at(DRing::Account::ConfProperties::USERNAME);
}

void
Agent::getConversations()
{
    conversations_ = DRing::getConversations(accountID_);
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

        AGENT_INFO("Incoming call from `%s`", peerDisplayName.c_str());

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
        DRing::sendMessage(accountID_, conversationID, msg, "");

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

bool
Agent::searchPeer()
{
    LOG_AGENT_STATE();

    std::set<std::string> peers;

    /* Prune contacts already friend with */
    for (auto it = peers_.begin(); it != peers_.end(); ++it) {
        bool prune = false;
        for (const auto& conv : conversations_) {
            if (conv == *it) {
                prune = true;
                break;
            }
        }
        if (not prune) {
            peers.emplace(*it);
        }
    }

    auto cv = std::make_shared<std::condition_variable>();

    for (auto it = peers.begin(); it != peers.end(); ++it) {
        DRing::sendTrustRequest(accountID_, it->c_str());
        DRing::subscribeBuddy(accountID_, it->c_str(), true);
    }

    if (conversations_.size()) {
        return true;
    }

    onContactAdded_.add([=](const std::string&, const std::string&, bool) {
        if (conversations_.size()) {
            cv->notify_one();
            return false;
        }
        return true;
    });

    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);

    cv->wait(lck);

    return true;
}

void
Agent::run(const std::string& yaml_config)
{
    static Agent agent;

    agent.initBehavior();
    agent.ensureAccount();
    agent.configure(yaml_config);
    agent.getConversations();
    agent.installSignalHandlers();
    agent.registerStaticCallbacks();

    if (agent.params_.size()) {
        while (true) {
            for (auto& cb : agent.params_) {
                cb();
                (*agent.root_)();
            }
        }
    } else {
        while ((*agent.root_) ()) {
            /* Until root fails */
        }
    }
}

/* Helper start here */
void
Agent::sendMessage(const std::string& to, const std::string& msg)
{
    auto parent = "";

    DRing::sendMessage(accountID_, to, msg, parent);
}

/* Behavior start here */

bool
Agent::echo()
{
    LOG_AGENT_STATE();

    if (conversations_.empty()) {
        return false;
    }

    auto it = conversations_.begin();

    std::advance(it, rand() % conversations_.size());

    auto cv = std::make_shared<std::condition_variable>();
    auto pongReceived = std::make_shared<std::atomic_bool>(false);
    auto to = *it;

    std::string alphabet = "0123456789ABCDEF";
    std::string messageSent;

    onMessageReceived_.add([=](const std::string& accountID,
                               const std::string& conversationID,
                               std::map<std::string, std::string> message) {
        (void) accountID;
        (void) conversationID;
        (void) message;
        (void) conversationID;

        if ("text/plain" != message.at("type")) {
            return true;
        }

        auto msg = message.at("body");

        if (pongReceived->load()) {
            return false;
        }

        if (to == message.at("author") and msg == messageSent) {
            *pongReceived = true;
            cv->notify_one();
            return false;
        }

        return true;
    });

    /* Sending msg */
    for (usize i = 0; i < 16; ++i) {
        messageSent.push_back(alphabet[rand() % alphabet.size()]);
    }

    sendMessage(*it, messageSent);

    /* Waiting for echo */

    std::mutex mutex;
    std::unique_lock<std::mutex> lck(mutex);

    bool ret = std::cv_status::no_timeout == cv->wait_for(lck, std::chrono::seconds(30))
               and pongReceived->load();

    return ret;
}

bool
Agent::makeCall()
{
    LOG_AGENT_STATE();

    if (conversations_.empty()) {
        return false;
    }

    auto it = conversations_.begin();

    std::advance(it, rand() % conversations_.size());

    auto cv = std::make_shared<std::condition_variable>();

    onCallStateChanged_.add([=](const std::string&, const std::string& state, signed) {
        if ("CURRENT" == state) {
            cv->notify_one();
            return false;
        }

        if ("OVER" == state) {
            return false;
        }

        return true;
    });

    auto members = DRing::getConversationMembers(accountID_, *it);

    std::string uri;

    for (const auto& member : members) {
        if (member.at("uri") != peerID_) {
            uri = member.at("uri");
            break;
        }
    }

    if (uri.empty()) {
        return false;
    }

    auto callID = DRing::placeCall(accountID_, uri);

    bool ret = true;

    std::mutex mtx;
    std::unique_lock<std::mutex> lck {mtx};

    if (std::cv_status::timeout == cv->wait_for(lck, std::chrono::seconds(30))) {
        ret = false;
    }

    DRing::hangUp(callID);

    return ret;
}

bool
Agent::wait()
{
    LOG_AGENT_STATE();

    std::this_thread::sleep_for(std::chrono::seconds(30));

    return true;
}
