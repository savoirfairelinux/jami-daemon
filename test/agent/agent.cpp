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
        BT::registered_tasks["wait"] = bind(&Agent::wait, &agent);
        BT::registered_tasks["ping-pong"] = bind(&Agent::pingPong, &agent);
        BT::registered_tasks["make-call"] = bind(&Agent::makeCall, &agent);
        BT::registered_tasks["end"] = bind(&Agent::end, &agent);
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

    /* Install handlers */
    {
        using namespace std::placeholders;
        using std::bind;

        std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> handlers;


        handlers.insert(DRing::exportable_callback<DRing::CallSignal::IncomingCallWithMedia>(
                                bind(&Agent::Handler<const std::string&,
                                     const std::string&,
                                     const std::string&,
                                     const std::vector<DRing::MediaMap>>::execute,
                                     &agent.incomingCall_, _1, _2, _3, _4)));

        handlers.insert(DRing::exportable_callback<DRing::CallSignal::StateChange>(
                                bind(&Agent::Handler<const std::string&, const std::string&, signed>::execute,
                                     &agent.callStateChanged_, _1, _2, _3)));

        handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingAccountMessage>(
                                bind(&Agent::Handler<const std::string&,
                                     const std::string&,
                                     const std::string&,
                                     const std::map<std::string, std::string>&>::execute,
                                     &agent.incomingAccountMessage_, _1, _2, _3, _4)));

        handlers.insert(DRing::exportable_callback<DRing::ConfigurationSignal::IncomingTrustRequest>(
                                bind(&Agent::Handler<const std::string&,
                                     const std::string&,
                                     const std::string&,
                                     const std::vector<uint8_t>&,
                                     time_t>::execute,
                                     &agent.incomingTrustRequest_, _1, _2, _3, _4, _5)));

        DRing::registerSignalHandlers(handlers);
    }

    /* Register callbacks */
    {
        agent.onIncomingCall([=](const std::string& accountId,
                                 const std::string& callId,
                                 const std::string& from,
                                 const std::vector<DRing::MediaMap> mediaList) {
            /* Why @ring.dht in `from`? */
            if (0 != agent.contacts_.count(from)) {
                assert(jami::Manager::instance().answerCallWithMedia(callId, mediaList));
            }

            return true;
        });

        agent.onCallStateChanged([=](const std::string& callId, const std::string& state, signed) {
            if ("CURRENT" != state) {
                goto out;
            }

            {
                auto call = jami::Manager::instance().getCallFromCallID(callId);
                auto details = agent.account_->getAccountDetails();

                /* TODO - This should be get from the agent accountd details */
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
            return true;
        });

        agent.onIncomingAccountMessage([=](const std::string&,
                                           const std::string& from,
                                           const std::string&,
                                           const std::map<std::string, std::string>& messages) {
            auto msg = messages.at("text/plain");

            if (0 == msg.compare(0, 5, "PING:")) {
                std::map<std::string, std::string> response;

                response["text/plain"] = "PONG:" + msg.substr(5);

                agent.account_->sendTextMessage(from, response);
            }

            return true;
        });

        agent.onIncomingTrustRequest([=](const std::string& accountID,
                                         const std::string&,
                                         const std::string& from,
                                         const std::vector<uint8_t>&,
                                         time_t) {
            for (const auto& peer : agent.peers_) {
                if (peer == from) {
                    assert(agent.account_->acceptTrustRequest(accountID, false));
                }
            }

            return true;
        });
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

    std::string alphabet = "0123456789ABCDEF";
    std::string messageSent = "PING:";

    for (usize i = 0; i < 16; ++i) {
        messageSent.push_back(alphabet[rand() % alphabet.size()]);
    }

    onIncomingAccountMessage([=](const std::string&,
                                 const std::string& from,
                                 const std::string&,
                                 const std::map<std::string, std::string>& messages) {
        auto msg = messages.at("text/plain");

        if (to == from and msg.substr(5) == messageSent.substr(5)) {
            *pongReceived_ = true;
            cv->notify_one();
            return false;
        }

        return true;
    });

    /* Sending ping */
    {
        std::map<std::string, std::string> msg;

        msg["text/plain"] = messageSent;

        account_->sendTextMessage(to, msg);
    }

    /* Waiting for Pong */
    {
        std::mutex mutex;
        std::unique_lock<std::mutex> lck(mutex);

        bool ret = std::cv_status::no_timeout == cv->wait_for(lck, std::chrono::seconds(30))
                   and pongReceived_->load();

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

    onCallStateChanged([=](const std::string&, const std::string& state, signed) {
        if (state == "ACTIVE") {
            cv->notify_one();
            return false;
        }

        return true;
    });

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

    std::this_thread::sleep_for(std::chrono::minutes(5 + rand() % 30));

    return true;
}
