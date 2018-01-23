/*
 *  Copyright (C) 2016-2018 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "message_engine.h"
#include "sip/sipaccountbase.h"
#include "manager.h"

#include "client/ring_signal.h"
#include "dring/account_const.h"

#include <json/json.h>

#include <fstream>

namespace ring {
namespace im {

static std::uniform_int_distribution<MessageToken> udist {1};
const std::chrono::minutes MessageEngine::RETRY_PERIOD = std::chrono::minutes(1);

MessageEngine::MessageEngine(SIPAccountBase& acc, const std::string& path) : account_(acc), savePath_(path)
{}

MessageToken
MessageEngine::sendMessage(const std::string& to, const std::map<std::string, std::string>& payloads)
{
    if (payloads.empty() or to.empty())
        return 0;
    MessageToken token;
    {
        std::lock_guard<std::mutex> lock(messagesMutex_);
        do {
            token = udist(account_.rand);
        } while (messages_.find(token) != messages_.end());
        auto m = messages_.emplace(token, Message{});
        m.first->second.to = to;
        m.first->second.payloads = payloads;
    }
    save();
    runOnMainThread([this]() {
        retrySend();
    });
    return token;
}

void
MessageEngine::reschedule()
{
    if (messages_.empty())
        return;
    std::weak_ptr<Account> w = std::static_pointer_cast<Account>(account_.shared_from_this());
    auto next = nextEvent();
    if (next != clock::time_point::max())
        Manager::instance().scheduleTask([w,this](){
            if (auto s = w.lock())
                retrySend();
        }, next);
}

MessageEngine::clock::time_point
MessageEngine::nextEvent() const
{
    auto next = clock::time_point::max();
    for (const auto& m : messages_) {
        if (m.second.status == MessageStatus::UNKNOWN || m.second.status == MessageStatus::IDLE) {
            auto next_op = m.second.last_op + RETRY_PERIOD;
            if (next_op < next)
                next = next_op;
        }
    }
    return next;
}

void
MessageEngine::retrySend()
{
    struct PendingMsg {
        MessageToken token;
        std::string to;
        std::map<std::string, std::string> payloads;
    };
    std::vector<PendingMsg> pending {};
    {
        std::lock_guard<std::mutex> lock(messagesMutex_);
        auto now = clock::now();
        for (auto m = messages_.begin(); m != messages_.end(); ++m) {
            if (m->second.status == MessageStatus::UNKNOWN || m->second.status == MessageStatus::IDLE) {
                auto next_op = m->second.last_op + RETRY_PERIOD;
                if (next_op <= now) {
                    m->second.status = MessageStatus::SENDING;
                    m->second.retried++;
                    m->second.last_op = clock::now();
                    pending.emplace_back(PendingMsg {m->first, m->second.to, m->second.payloads});
                }
            }
        }
    }
    // avoid locking while calling callback
    for (auto& p : pending) {
        RING_DBG("[message %" PRIx64 "] retrying sending", p.token);
        emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(
            account_.getAccountID(),
            p.token,
            p.to,
            (int)DRing::Account::MessageStates::SENDING);
        account_.sendTextMessage(p.to, p.payloads, p.token);
    }
}

MessageStatus
MessageEngine::getStatus(MessageToken t) const
{
    std::lock_guard<std::mutex> lock(messagesMutex_);
    const auto m = messages_.find(t);
    return (m == messages_.end()) ? MessageStatus::UNKNOWN : m->second.status;
}

void
MessageEngine::onMessageSent(MessageToken token, bool ok)
{
    RING_DBG("[message %" PRIx64 "] message sent: %s", token, ok ? "success" : "failure");
    std::lock_guard<std::mutex> lock(messagesMutex_);
    auto f = messages_.find(token);
    if (f != messages_.end()) {
        if (f->second.status == MessageStatus::SENDING) {
            if (ok) {
                f->second.status = MessageStatus::SENT;
                RING_DBG("[message %" PRIx64 "] status changed to SENT", token);
                emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(account_.getAccountID(),
                                                                             token,
                                                                             f->second.to,
                                                                             static_cast<int>(DRing::Account::MessageStates::SENT));
            } else if (f->second.retried >= MAX_RETRIES) {
                f->second.status = MessageStatus::FAILURE;
                RING_WARN("[message %" PRIx64 "] status changed to FAILURE", token);
                emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(account_.getAccountID(),
                                                                             token,
                                                                             f->second.to,
                                                                             static_cast<int>(DRing::Account::MessageStates::FAILURE));
            } else {
                f->second.status = MessageStatus::IDLE;
                RING_DBG("[message %" PRIx64 "] status changed to IDLE", token);
                reschedule();
            }
        }
        else {
           RING_DBG("[message %" PRIx64 "] state is not SENDING", token);
        }
    }
    else {
        RING_DBG("[message %" PRIx64 "] can't find message", token);
    }
}

void
MessageEngine::load()
{
    std::lock_guard<std::mutex> lock(messagesMutex_);
    try {
        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(savePath_);

        Json::Value root;
        file >> root;

        long unsigned loaded {0};
        for (auto i = root.begin(); i != root.end(); ++i) {
            const auto& jmsg = *i;
            MessageToken token;
            std::istringstream iss(i.key().asString());
            iss >> std::hex >> token;
            Message msg;
            msg.status = (MessageStatus)jmsg["status"].asInt();
            msg.to = jmsg["to"].asString();
            auto wall_time = std::chrono::system_clock::from_time_t(jmsg["last_op"].asInt64());
            msg.last_op = clock::now() + (wall_time - std::chrono::system_clock::now());
            msg.retried = jmsg.get("retried", 0).asUInt();
            const auto& pl = jmsg["payload"];
            for (auto p = pl.begin(); p != pl.end(); ++p)
                msg.payloads[p.key().asString()] = p->asString();
           messages_.emplace(token, std::move(msg));
           loaded++;
        }
        RING_DBG("[Account %s] loaded %lu messages from %s", account_.getAccountID().c_str(), loaded, savePath_.c_str());

        // everything whent fine, removing the file
        std::remove(savePath_.c_str());
    } catch (const std::exception& e) {
        RING_ERR("[Account %s] couldn't load messages from %s: %s", account_.getAccountID().c_str(), savePath_.c_str(), e.what());
    }
    reschedule();
}

void
MessageEngine::save() const
{
    try {
        Json::Value root(Json::objectValue);
        std::unique_lock<std::mutex> lock(messagesMutex_);
        for (auto& c : messages_) {
            auto& v = c.second;
            Json::Value msg;
            std::ostringstream msgsId;
            msgsId << std::hex << c.first;
            msg["status"] = (int)(v.status == MessageStatus::SENDING ? MessageStatus::IDLE : v.status);
            msg["to"] = v.to;
            auto wall_time = std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>(v.last_op - clock::now());
            msg["last_op"] = (Json::Value::Int64) std::chrono::system_clock::to_time_t(wall_time);
            msg["retried"] = v.retried;
            auto& payloads = msg["payload"];
            for (const auto& p : v.payloads)
                payloads[p.first] = p.second;
            root[msgsId.str()] = std::move(msg);
        }
        lock.unlock();
        std::ofstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(savePath_, std::ios::trunc);
        Json::StreamWriterBuilder wbuilder;
        wbuilder["commentStyle"] = "None";
        wbuilder["indentation"] = "";
        file << Json::writeString(wbuilder, root);
        RING_DBG("[Account %s] saved %lu messages to %s", account_.getAccountID().c_str(), messages_.size(), savePath_.c_str());
    } catch (const std::exception& e) {
        RING_ERR("[Account %s] couldn't save messages to %s: %s", account_.getAccountID().c_str(), savePath_.c_str(), e.what());
    }
}

}}
