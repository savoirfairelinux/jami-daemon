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
#include <memory>

#include <yaml-cpp/yaml.h>

/* Local */
#include "agent/agent.h"
#include "agent/bt.h"
#include "agent/utils.h"

/* Jami */
#include "account_const.h"
#include "fileutils.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"

#include "sip/sipcall.h"

using usize = size_t;

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
Agent::initBehavior(void)
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
    std::ifstream file = jami::fileutils::ifstream(yaml_config);

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
        auto account_details = account_->getAccountDetails();
        assert(params.IsSequence());
        for (const auto& param : params) {
            assert(param.IsSequence());
            for (const auto& details : param) {
                assert(details.IsMap());
                for (const auto& detail : details) {
                    auto first = detail.first.as<std::string>();
                    auto second = detail.second.as<std::string>();
                    account_details["Account." + first] = second;
                }
            }
            params_.emplace_back([this, account_details = std::move(account_details)] {
                // Method is private
                // account_->setAccountDetails(account_details);
            });
        }
    }
}

void
Agent::ensureAccount(void)
{
    auto& manager = jami::Manager::instance();
    auto accounts = manager.getAccountList();

    AGENT_INFO("Ensuring account exists");

    if (0 == accounts.size()) {
        std::map<std::string, std::string> details;

        details[DRing::Account::ConfProperties::TYPE] = "RING";
        details[DRing::Account::ConfProperties::DISPLAYNAME] = "AGENT";
        details[DRing::Account::ConfProperties::ALIAS] = "AGENT";
        details[DRing::Account::ConfProperties::ARCHIVE_PASSWORD] = "";
        details[DRing::Account::ConfProperties::ARCHIVE_PIN] = "";
        details[DRing::Account::ConfProperties::ARCHIVE_PATH] = "";

        accountId_ = manager.addAccount(details);
    } else {
        assert(1 == accounts.size());
        accountId_ = accounts[0];
    }

    account_ = manager.getAccount<jami::JamiAccount>(accountId_);

    wait_for_announcement_of(account_->getAccountID());
}

void
Agent::createContacts(void)
{
    for (const auto& contact : account_->getContacts()) {
        if ("true" == contact.at("confirmed")) {
            auto id = contact.at("id");
            for (const auto& peer : peers_) {
                if (peer == id) {
                    contacts_.emplace(id);
                    break;
                }
            }
        }
    }

    AGENT_INFO("Found %zu contacts", contacts_.size());
}

void
Agent::installSignalHandlers(void)
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

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingAccountMessage>(
        bind(&Agent::Handler<const std::string&,
                             const std::string&,
                             const std::string&,
                             const std::map<std::string, std::string>&>::execute,
             &onIncomingAccountMessage_,
             _1,
             _2,
             _3,
             _4)));

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingTrustRequest>(
        bind(&Agent::Handler<const std::string&,
                             const std::string&,
                             const std::string&,
                             const std::vector<uint8_t>&,
                             time_t>::execute,
             &onIncomingTrustRequest_,
             _1,
             _2,
             _3,
             _4,
             _5)));

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::ContactAdded>(
        bind(&Agent::Handler<const std::string&, const std::string&, bool>::execute,
             &onContactAdded_,
             _1,
             _2,
             _3)));

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::ContactRemoved>(
        bind(&Agent::Handler<const std::string&, const std::string&, bool>::execute,
             &onContactRemoved_,
             _1,
             _2,
             _3)));

    DRing::registerSignalHandlers(handlers);
}

void
Agent::registerStaticCallbacks(void)
{
    onIncomingCall_.add([=](const std::string& accountId,
                            const std::string& callId,
                            const std::string& from,
                            const std::vector<DRing::MediaMap> mediaList) {
        AGENT_INFO("Incoming call from `%s`", from.c_str());

        /* Why @ring.dht in `from`? */
        // if (0 != contacts_.count(from)) {
        if (true) {
            assert(jami::Manager::instance().answerCallWithMedia(callId, mediaList));
        }

        return true;
    });

    onIncomingAccountMessage_.add([=](const std::string&,
                                      const std::string& from,
                                      const std::string&,
                                      const std::map<std::string, std::string>& messages) {
        std::map<std::string, std::string> response;

        auto msg = messages.at("text/plain");

        AGENT_INFO("Incomming message `%s` from %s", msg.c_str(), from.c_str());

        response["text/plain"] = messages.at("text/plain");

        account_->sendTextMessage(from, response);

        return true;
    });

    onIncomingTrustRequest_.add([=](const std::string& accountID,
                                    const std::string&,
                                    const std::string& from,
                                    const std::vector<uint8_t>&,
                                    time_t) {
        for (const auto& peer : peers_) {
            if (peer == from) {
                JAMI_INFO("Accepted incoming trust request from %s", from.c_str());
                assert(account_->acceptTrustRequest(from, true));
            } else {
                assert(account_->discardTrustRequest(from));
            }
        }

        JAMI_INFO("Refused incoming trust request from `%s`", from.c_str());

        return true;
    });

    onContactAdded_.add([=](const std::string& accountID, const std::string& URI, bool confirmed) {
        AGENT_INFO("Contact added `%s` : %s", URI.c_str(), confirmed ? "accepted" : "refused");
        if (confirmed) {
            contacts_.emplace(URI);
        }
        return true;
    });

    onContactRemoved_.add([=](const std::string& accountID, const std::string& URI, bool banned) {
        AGENT_INFO("Contact removed `%s` : %s", URI.c_str(), banned ? "banned" : "delete");
        contacts_.erase(URI);
        return true;
    });
}

bool
Agent::searchPeer(void)
{
    LOG_AGENT_STATE();

    std::set<std::string> peers;

    /* Prune contacts already friend with */
    for (auto it = peers_.begin(); it != peers_.end(); ++it) {
        if (0 == contacts_.count(*it)) {
            peers.emplace(*it);
        }
    }

    auto cv = std::make_shared<std::condition_variable>();

    /* TODO - What's to pass as second argument? */
    for (auto it = peers.begin(); it != peers.end(); ++it) {
        AGENT_INFO("Sending trust request to `%s`", it->c_str());
        account_->sendTrustRequest(*it, {'H', 'E', 'L', 'L', 'O'});
    }

    if (contacts_.size()) {
        return true;
    }

    onContactAdded_.add([=](const std::string&, const std::string&, bool confirmed) {
        if (contacts_.size()) {
            cv->notify_one();
            return false;
        }
        return true;
    });

    {
        std::mutex mtx;
        std::unique_lock<std::mutex> lck(mtx);

        cv->wait(lck);
    }

    return true;
}

void
Agent::run(const std::string& yaml_config)
{
    static Agent agent;

    agent.initBehavior();
    agent.ensureAccount();
    agent.configure(yaml_config);
    agent.createContacts();
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

bool
Agent::echo(void)
{
    LOG_AGENT_STATE();

    if (contacts_.empty()) {
        return false;
    }

    auto it = contacts_.begin();

    std::advance(it, rand() % contacts_.size());

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> handlers;

    auto cv = std::make_shared<std::condition_variable>();
    auto pongReceived = std::make_shared<std::atomic_bool>(false);
    auto to = *it;

    std::string alphabet = "0123456789ABCDEF";
    std::string messageSent;

    for (usize i = 0; i < 16; ++i) {
        messageSent.push_back(alphabet[rand() % alphabet.size()]);
    }

    onIncomingAccountMessage_.add([=](const std::string&,
                                      const std::string& from,
                                      const std::string&,
                                      const std::map<std::string, std::string>& messages) {
        auto msg = messages.at("text/plain");

        if (to == from and msg == messageSent) {
            *pongReceived = true;
            cv->notify_one();
            return false;
        }

        return true;
    });

    /* Sending msg */
    {
        std::map<std::string, std::string> msg;

        msg["text/plain"] = messageSent;

        account_->sendTextMessage(to, msg);
    }

    /* Waiting for echo */
    {
        std::mutex mutex;
        std::unique_lock<std::mutex> lck(mutex);

        bool ret = std::cv_status::no_timeout == cv->wait_for(lck, std::chrono::seconds(30))
                   and pongReceived->load();

        return ret;
    }
}

bool
Agent::makeCall(void)
{
    LOG_AGENT_STATE();

    if (contacts_.empty()) {
        return false;
    }

    auto it = contacts_.begin();

    std::advance(it, rand() % contacts_.size());

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

#if 0
    /* The inner logic is okay, but we can't actually touch `onConnectionReady` */
    account_->connectionManager().onConnectionReady([=](const jami::DeviceId&, const std::string&, std::shared_ptr<jami::ChannelSocket> channel){

            if (not channel) {
                    return;
            }

            /* TODO - This should be get from the agent accountd details */
            auto details = account_->getAccountDetails();
            bool upnp_enabled = true;

            auto ice_transport = channel->underlyingICE();

            assert(ice_transport);

            auto pair = ice_transport->getValidPair(1);

            assert("?" != pair.first and "?" != pair.second);

            if (upnp_enabled) {
                    assert("relayed" != pair.first);
                    assert("relayed" != pair.second);
            }
            cv->notify_one();
    });
#endif

    auto call = account_->newOutgoingCall(*it);
    bool ret = true;

    {
        std::mutex mtx;
        std::unique_lock<std::mutex> lck {mtx};

        if (std::cv_status::timeout == cv->wait_for(lck, std::chrono::seconds(30))) {
            ret = false;
        }
    }

    jami::Manager::instance().hangupCall(call->getCallId());

    return ret;
}

bool
Agent::wait(void)
{
    LOG_AGENT_STATE();

    std::this_thread::sleep_for(std::chrono::seconds(30));

    return true;
}

/*
 * + plus de log (periodique)
 * + DRING_TESTABLE
 * + Refactoring
 */
