/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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

static std::uniform_int_distribution<uint64_t> udist {1};

MessageEngine::MessageEngine(SIPAccountBase& acc, const std::string& path) : account_(acc), savePath_(path)
{}

MessageToken
MessageEngine::sendMessage(const std::string& to, const std::map<std::string, std::string>& payloads)
{
    if (payloads.empty() or to.empty())
        return 0;
    auto token = udist(account_.rand_);
    {
        std::lock_guard<std::mutex> lock(messagesMutex_);
        auto m = messages_.emplace(token, Message{});
        m.first->second.to = to;
        m.first->second.payloads = payloads;
    }
    save();
    runOnMainThread([this,token]() {
        std::lock_guard<std::mutex> lock(messagesMutex_);
        auto m = messages_.find(token);
        if (m != messages_.end())
            trySend(m);
    });
    return token;
}

MessageStatus
MessageEngine::getStatus(MessageToken t) const
{
    std::lock_guard<std::mutex> lock(messagesMutex_);
    const auto m = messages_.find(t);
    return (m == messages_.end()) ? MessageStatus::UNKNOWN : m->second.status;
}

void
MessageEngine::trySend(decltype(MessageEngine::messages_)::iterator m)
{
    RING_WARN("MessageEngine::trySend %lu", m->first);
    if (m->second.status != MessageStatus::IDLE &&
        m->second.status != MessageStatus::UNKNOWN) {
        RING_WARN("Can't send message in status %d", (int)m->second.status);
        return;
    }
    m->second.status = MessageStatus::SENDING;
    m->second.retried++;
    m->second.last_op = std::chrono::system_clock::now();
    emitSignal<DRing::ConfigurationSignal::AccountMessageStatus>(m->first, DRing::Account::MessageStates::SENDING);
    account_.sendTextMessage(m->second.to, m->second.payloads, m->first);
}

void
MessageEngine::onMessageSent(MessageToken token, bool ok)
{
    RING_WARN("sendTextMessage callback %d", ok);
    std::lock_guard<std::mutex> lock(messagesMutex_);
    auto f = messages_.find(token);
    if (f != messages_.end()) {
        if (f->second.status == MessageStatus::SENDING) {
            if (ok) {
                f->second.status = MessageStatus::SENT;
                emitSignal<DRing::ConfigurationSignal::AccountMessageStatus>(token, DRing::Account::MessageStates::SENT);
            } else if (f->second.retried == MAX_RETRIES) {
                f->second.status = MessageStatus::FAILURE;
                emitSignal<DRing::ConfigurationSignal::AccountMessageStatus>(token, DRing::Account::MessageStates::FAILURE);
            } else {
                f->second.status = MessageStatus::IDLE;
                // TODO: reschedule sending
            }
        }
    }
}

void
MessageEngine::load()
{
    try {
        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(savePath_);

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(file, root))
            throw std::runtime_error("can't parse JSON.");

        std::lock_guard<std::mutex> lock(messagesMutex_);

        if (not root.isArray())
            throw std::runtime_error("can't parse JSON: root must be an array");

        for (int i = 0, n = root.size(); i < n ; ++i) {
            const auto& jmsg = root[i];
            MessageToken token;
            std::istringstream iss(jmsg["id"].asString());
            iss >> std::hex >> token;
            Message msg;
            msg.status = (MessageStatus)jmsg["status"].asInt();
            msg.to = jmsg["to"].asString();
            msg.last_op = std::chrono::system_clock::from_time_t(jmsg["last_op"].asInt64());
            const auto& pl = jmsg["payload"];
            for (auto i = pl.begin(); i != pl.end(); ++i)
                msg.payloads[i.key().asString()] = i->asString();
           auto m = messages_.emplace(token, std::move(msg));
        }

        // everything whent fine, removing the file
        std::remove(savePath_.c_str());
    } catch (const std::exception& e) {
        RING_ERR("Could not load messages from %s: %s", savePath_.c_str(), e.what());
    }
}

void
MessageEngine::save() const
{
    try {
        Json::Value root(Json::arrayValue);
        unsigned i = 0;
        std::unique_lock<std::mutex> lock(messagesMutex_);
        for (auto& c : messages_) {
            auto& v = c.second;
            Json::Value msg;
            std::ostringstream msgsId;
            msgsId << std::hex << c.first;
            msg["id"] = msgsId.str();
            msg["status"] = (int)(v.status == MessageStatus::SENDING ? MessageStatus::IDLE : v.status);
            msg["to"] = v.to;
            msg["last_op"] = (Json::Value::Int64) std::chrono::system_clock::to_time_t(v.last_op);
            auto& payloads = msg["payload"];
            for (const auto& p : v.payloads)
                payloads[p.first] = p.second;
            root[i++] = std::move(msg);
        }
        lock.unlock();
        Json::FastWriter fastWriter;
        std::ofstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(savePath_, std::ios::trunc);
        file << fastWriter.write(root);
    } catch (const std::exception& e) {
        RING_ERR("Could not save messages to %s: %s", savePath_.c_str(), e.what());
    }
}

}}
