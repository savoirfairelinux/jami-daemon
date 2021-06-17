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

/* Third parties */
#include <pjnath.h>

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

#define private public
#include "sip/sipcall.h"
#undef private

using usize = size_t;

#define LOG_AGENT_STATE() JAMI_INFO("AGENT: In state %s", __FUNCTION__)

void
Agent::run(const std::string& yaml_config)
{
    static Agent agent;
    std::unique_ptr<BT::Node> root;

    /* Register behavior */
    {
        using std::bind;

        BT::registered_tasks["search-peer"] = bind(&Agent::searchPeer, &agent);
        BT::registered_tasks["wait-for-event"] = bind(&Agent::waitForEvent, &agent);
        BT::registered_tasks["ping-pong"] = bind(&Agent::pingPong, &agent);
        BT::registered_tasks["make-call"] = bind(&Agent::makeCall, &agent);
    }

    /* Configuration */
    {
        std::ifstream file = jami::fileutils::ifstream(yaml_config);

        assert(file.is_open());

        YAML::Node node = YAML::Load(file);

        auto peers = node["peers"];

        assert(peers.IsSequence());

        for (const auto& peer : peers) {
            agent.peers_.emplace_back(peer.as<std::string>());
        }

        root = BT::from_yaml(node["behavior"]);
    }

    /* Create account and wait for its announcement on the DHT */
    {
        auto& manager = jami::Manager::instance();
        auto accounts = manager.getAccountList();

        if (0 == accounts.size()) {
            std::map<std::string, std::string> details;

            details[DRing::Account::ConfProperties::TYPE] = "RING";
            details[DRing::Account::ConfProperties::DISPLAYNAME] = "AGENT";
            details[DRing::Account::ConfProperties::ALIAS] = "AGENT";
            details[DRing::Account::ConfProperties::ARCHIVE_PASSWORD] = "";
            details[DRing::Account::ConfProperties::ARCHIVE_PIN] = "";
            details[DRing::Account::ConfProperties::ARCHIVE_PATH] = "";
            details[DRing::Account::ConfProperties::UPNP_ENABLED] = "true";
            details[DRing::Account::ConfProperties::TURN::ENABLED] = "false";

            agent.accountId_ = manager.addAccount(details);
        } else {
            assert(1 == accounts.size());
            agent.accountId_ = accounts[0];
        }

        agent.account_ = manager.getAccount<jami::JamiAccount>(agent.accountId_);

        for (const auto& contact : agent.account_->getContacts()) {
            if ("true" == contact.at("confirmed")) {
                agent.contacts_.emplace(contact.at("id"));
            }
        }

        wait_for_announcement_of(agent.account_->getAccountID());
    }

    while ((*root)()) {
        /* Until root fails */
    }
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

    /* TODO - What's to pass as second argument? */
    for (auto it = peers.begin(); it != peers.end(); ++it) {
        account_->sendTrustRequest(*it, {'H', 'E', 'L', 'L', 'O'});
    }

    return true;
}

bool
Agent::pingPong(void)
{
    LOG_AGENT_STATE();

    if (contacts_.empty()) {
        return false;
    }

    auto it = contacts_.begin();

    std::advance(it, rand() % contacts_.size());

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> handlers;

    auto cv = std::make_shared<std::condition_variable>();
    auto pongReceived_ = std::make_shared<std::atomic_bool>(false);
    auto to = *it;

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingAccountMessage>(
        [=](const std::string&,
            const std::string& from,
            const std::string&,
            const std::map<std::string, std::string>& messages) {
            if ("PING" == messages.at("text/plain")) {
                std::map<std::string, std::string> response;

                response["text/plain"] = "PONG";

                account_->sendTextMessage(from, response);
            } else if (to == from and "PONG" == messages.at("text/plain")) {
                *pongReceived_ = true;
                cv->notify_one();
            }
        }));

    DRing::registerSignalHandlers(handlers);

    /* Sending ping */
    {
        std::map<std::string, std::string> msg;

        msg["text/plain"] = "PING";

        account_->sendTextMessage(to, msg);
    }

    /* Waiting for Pong */
    {
        std::mutex mutex;
        std::unique_lock<std::mutex> lck(mutex);

        bool ret = std::cv_status::no_timeout == cv->wait_for(lck, std::chrono::seconds(30))
                   and pongReceived_->load();

        DRing::unregisterSignalHandlers();

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

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;

    confHandlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [=](const std::string&, const std::string& state, signed) {
            if (state == "ACTIVE") {
                cv->notify_one();
            }
        }));

    DRing::registerSignalHandlers(confHandlers);

    auto call = account_->newOutgoingCall(*it);
    bool ret = true;

    {
        std::mutex mtx;
        std::unique_lock<std::mutex> lck {mtx};

        if (std::cv_status::timeout == cv->wait_for(lck, std::chrono::seconds(30))) {
            ret = false;
        }
    }

    DRing::unregisterSignalHandlers();

    jami::Manager::instance().hangupCall(call->getCallId());

    return ret;
}

bool
Agent::waitForEvent(void)
{
    LOG_AGENT_STATE();

    /* Note that all of this should be permanent */

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> handlers;

    auto cv = std::make_shared<std::condition_variable>();
    auto pending = std::make_shared<std::atomic<usize>>(0);

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingAccountMessage>(
        [=](const std::string&,
            const std::string& from,
            const std::string&,
            const std::map<std::string, std::string>& messages) {
            *pending += 1;
            if ("PING" == messages.at("text/plain")) {
                std::map<std::string, std::string> response;

                response["text/plain"] = "PONG";

                account_->sendTextMessage(from, response);
            }
            *pending -= 1;
            cv->notify_one();
        }));

    handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingTrustRequest>(
        [=](const std::string& accountID,
            const std::string&,
            const std::string& from,
            const std::vector<uint8_t>&,
            time_t) {
            *pending += 1;

            for (const auto& peer : peers_) {
                if (peer == from) {
                    assert(account_->acceptTrustRequest(accountID, false));
                }
            }

            *pending -= 1;

            cv->notify_one();
        }));

    handlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCallWithMedia>(
        [=](const std::string& accountId,
            const std::string& callId,
            const std::string& from,
            const std::vector<DRing::MediaMap> mediaList) {
            *pending += 1;

        /* Why @ring.dht in `from`? */
#if 0
                                        if (0 == contacts_.count(from)) {
                                                goto out;
                                        }
#endif

            assert(jami::Manager::instance().answerCallWithMedia(callId, mediaList));

        out:
            *pending -= 1;
            cv->notify_one();
        }));

    handlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
        [=](const std::string& callId, const std::string& state, signed) {
            *pending += 1;
            if ("CURRENT" != state) {
                goto out;
            }

            {
                auto call = jami::Manager::instance().getCallFromCallID(callId);
                auto details = account_->getAccountDetails();

                /* TODO - This should be get from the agent account */
                bool upnp_enabled = true;

                auto ice_transport = std::dynamic_pointer_cast<jami::SIPCall>(call)
                                         ->getIceMediaTransport();

                assert(ice_transport);

                /* Always return nullptr :-/ */
                auto pair = ice_transport->getValidPair(1);

                assert(pair);

                if (upnp_enabled) {
                    assert(PJ_ICE_CAND_TYPE_RELAYED != pair->lcand->type);
                    assert(PJ_ICE_CAND_TYPE_RELAYED != pair->rcand->type);
                }
            }
        out:
            *pending -= 1;
            cv->notify_one();
        }));

    DRing::registerSignalHandlers(handlers);

    std::mutex mutex;
    std::unique_lock<std::mutex> lck(mutex);

    cv->wait_for(lck, std::chrono::minutes(5 + rand() % 30), [=] { return 0 != pending->load(); });

    DRing::unregisterSignalHandlers();

    return true;
}
