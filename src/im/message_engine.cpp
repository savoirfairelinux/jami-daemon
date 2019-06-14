/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
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
#include "fileutils.h"

#include "client/ring_signal.h"
#include "dring/account_const.h"

#include <opendht/thread_pool.h>
#include <json/json.h>

#include <fstream>

namespace jami {
namespace im {

static std::uniform_int_distribution<MessageToken> udist {1};

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
        auto& peerMessages = messages_[to];
        do {
            token = udist(account_.rand);
        } while (peerMessages.find(token) != peerMessages.end());
        auto m = peerMessages.emplace(token, Message{});
        m.first->second.to = to;
        m.first->second.payloads = payloads;
        save_();
    }
    runOnMainThread([this, to]() {
        retrySend(to);
    });
    return token;
}

void
MessageEngine::onPeerOnline(const std::string& peer)
{
    retrySend(peer);
}

void
MessageEngine::retrySend(const std::string& peer)
{
    struct PendingMsg {
        MessageToken token;
        std::string to;
        std::map<std::string, std::string> payloads;
    };
    std::vector<PendingMsg> pending {};
    {
        std::lock_guard<std::mutex> lock(messagesMutex_);
        auto p = messages_.find(peer);
        if (p == messages_.end())
            return;
        auto& messages = p->second;
        for (auto m = messages.begin(); m != messages.end(); ++m) {
            if (m->second.status == MessageStatus::UNKNOWN || m->second.status == MessageStatus::IDLE) {
                m->second.status = MessageStatus::SENDING;
                m->second.retried++;
                m->second.last_op = clock::now();
                pending.emplace_back(PendingMsg {m->first, m->second.to, m->second.payloads});
            }
        }
    }
    // avoid locking while calling callback
    for (const auto& p : pending) {
        JAMI_DBG() << "[message " << p.token << "] Retry sending";
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
    for (const auto& p : messages_) {
        const auto m = p.second.find(t);
        if (m != p.second.end())
            return m->second.status;
    }
    return MessageStatus::UNKNOWN;
}

bool
MessageEngine::cancel(MessageToken t)
{
    std::lock_guard<std::mutex> lock(messagesMutex_);
    for (auto& p : messages_) {
        auto m = p.second.find(t);
        if (m != p.second.end()) {
            m->second.status = MessageStatus::CANCELLED;
            emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(account_.getAccountID(),
                                                                            t,
                                                                            m->second.to,
                                                                            static_cast<int>(DRing::Account::MessageStates::CANCELLED));
            save_();
            return true;
        }
    }
    return false;
}

void
MessageEngine::onMessageSent(const std::string& peer, MessageToken token, bool ok)
{
    JAMI_DBG() << "[message " << token << "] Message sent: " << (ok ? "success" : "failure");
    std::lock_guard<std::mutex> lock(messagesMutex_);
    auto p = messages_.find(peer);
    if (p == messages_.end()) {
        JAMI_DBG() << "[message " << token << "] Can't find peer";
        return;
    }
    auto f = p->second.find(token);
    if (f != p->second.end()) {
        if (f->second.status == MessageStatus::SENDING) {
            if (ok) {
                f->second.status = MessageStatus::SENT;
                JAMI_DBG() << "[message " << token << "] Status changed to SENT";
                emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(account_.getAccountID(),
                                                                             token,
                                                                             f->second.to,
                                                                             static_cast<int>(DRing::Account::MessageStates::SENT));
                save_();
            } else if (f->second.retried >= MAX_RETRIES) {
                f->second.status = MessageStatus::FAILURE;
                JAMI_DBG() << "[message " << token << "] Status changed to FAILURE";
                emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(account_.getAccountID(),
                                                                             token,
                                                                             f->second.to,
                                                                             static_cast<int>(DRing::Account::MessageStates::FAILURE));
                save_();
            } else {
                f->second.status = MessageStatus::IDLE;
                JAMI_DBG() << "[message " << token << "] Status changed to IDLE";
            }
        } else {
           JAMI_DBG() << "[message " << token << "] State is not SENDING";
        }
    } else {
        JAMI_DBG() << "[message " << token << "] Can't find message";
    }
}

void
MessageEngine::load()
{
    try {
        Json::Value root;
        {
            std::lock_guard<std::mutex> lock(fileutils::getFileLock(savePath_));
            std::ifstream file;
            file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
            file.open(savePath_);
            file >> root;
        }
        std::lock_guard<std::mutex> lock(messagesMutex_);
        long unsigned loaded {0};
        for (auto i = root.begin(); i != root.end(); ++i) {
            auto to = i.key().asString();
            auto& pmessages = *i;
            auto& p = messages_[to];
            for (auto m = pmessages.begin(); m != pmessages.end(); ++m) {
                const auto& jmsg = *m;
                MessageToken token;
                std::istringstream iss(m.key().asString());
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
                p.emplace(token, std::move(msg));
                loaded++;
            }
        }
        JAMI_DBG("[Account %s] loaded %lu messages from %s", account_.getAccountID().c_str(), loaded, savePath_.c_str());
    } catch (const std::exception& e) {
        JAMI_DBG("[Account %s] couldn't load messages from %s: %s", account_.getAccountID().c_str(), savePath_.c_str(), e.what());
    }
}

void
MessageEngine::save() const
{
    std::lock_guard<std::mutex> lock(messagesMutex_);
    save_();
}

void
MessageEngine::save_() const
{
    try {
        Json::Value root(Json::objectValue);
        for (auto& c : messages_) {
            Json::Value peerRoot(Json::objectValue);
            for (auto& m : c.second) {
                auto& v = m.second;
                if (v.status == MessageStatus::FAILURE || v.status == MessageStatus::SENT || v.status == MessageStatus::CANCELLED)
                    continue;
                Json::Value msg;
                std::ostringstream msgsId;
                msgsId << std::hex << m.first;
                msg["status"] = (int)(v.status == MessageStatus::SENDING ? MessageStatus::IDLE : v.status);
                msg["to"] = v.to;
                auto wall_time = std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>(v.last_op - clock::now());
                msg["last_op"] = (Json::Value::Int64) std::chrono::system_clock::to_time_t(wall_time);
                msg["retried"] = v.retried;
                auto& payloads = msg["payload"];
                for (const auto& p : v.payloads)
                    payloads[p.first] = p.second;
                peerRoot[msgsId.str()] = std::move(msg);
            }
            root[c.first] = std::move(peerRoot);
        }
        // Save asynchronously
        dht::ThreadPool::computation().run([path = savePath_,
                                    root = std::move(root),
                                    accountID = account_.getAccountID(),
                                    messageNum = messages_.size()]
        {
            std::lock_guard<std::mutex> lock(fileutils::getFileLock(path));
            try {
                Json::StreamWriterBuilder wbuilder;
                wbuilder["commentStyle"] = "None";
                wbuilder["indentation"] = "";
                const std::unique_ptr<Json::StreamWriter> writer(wbuilder.newStreamWriter());
                std::ofstream file;
                file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                file.open(path, std::ios::trunc);
                writer->write(root, &file);
            } catch (const std::exception& e) {
                JAMI_ERR("[Account %s] Couldn't save messages to %s: %s", accountID.c_str(), path.c_str(), e.what());
            }
            JAMI_DBG("[Account %s] saved %zu messages to %s", accountID.c_str(), messageNum, path.c_str());
        });
    } catch (const std::exception& e) {
        JAMI_ERR("[Account %s] couldn't save messages to %s: %s", account_.getAccountID().c_str(), savePath_.c_str(), e.what());
    }
}

}}
