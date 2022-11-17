/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#include "jami/account_const.h"

#include <opendht/thread_pool.h>
#include <json/json.h>

#include <fstream>

namespace jami {
namespace im {

MessageEngine::MessageEngine(SIPAccountBase& acc, const std::string& path)
    : account_(acc)
    , savePath_(path)
{
    auto found = savePath_.find_last_of(DIR_SEPARATOR_CH);
    auto dir = savePath_.substr(0, found);
    fileutils::check_dir(dir.c_str());
}

MessageToken
MessageEngine::sendMessage(const std::string& to,
                           const std::string& deviceId,
                           const std::map<std::string, std::string>& payloads,
                           uint64_t refreshToken)
{
    if (payloads.empty() or to.empty())
        return 0;
    MessageToken token;
    {
        std::lock_guard<std::mutex> lock(messagesMutex_);

        auto& peerMessages = deviceId.empty() ? messages_[to] : messagesDevices_[deviceId];
        auto previousIt = peerMessages.find(refreshToken);
        if (previousIt != peerMessages.end() && previousIt->second.status != MessageStatus::SENT) {
            JAMI_DEBUG("[message {:d}] Replace content", refreshToken);
            token = refreshToken;
            previousIt->second.to = to;
            previousIt->second.payloads = payloads;
        } else {
            do {
                token = std::uniform_int_distribution<MessageToken> {1, JAMI_ID_MAX_VAL}(
                    account_.rand);
            } while (peerMessages.find(token) != peerMessages.end());
            auto m = peerMessages.emplace(token, Message {});
            m.first->second.to = to;
            m.first->second.payloads = payloads;
        }
        save_();
    }
    runOnMainThread([this, to, deviceId]() { retrySend(to, true, deviceId); });
    return token;
}

void
MessageEngine::onPeerOnline(const std::string& peer,
                            bool retryOnTimeout,
                            const std::string& deviceId)
{
    retrySend(peer, retryOnTimeout, deviceId);
}

void
MessageEngine::retrySend(const std::string& peer, bool retryOnTimeout, const std::string& deviceId)
{
    if (account_.getRegistrationState() != RegistrationState::REGISTERED)
        return;
    struct PendingMsg
    {
        MessageToken token;
        std::string to;
        std::map<std::string, std::string> payloads;
    };
    std::vector<PendingMsg> pending {};
    {
        std::lock_guard<std::mutex> lock(messagesMutex_);

        auto& m = deviceId.empty() ? messages_ : messagesDevices_;
        auto p = m.find(deviceId.empty() ? peer : deviceId);
        if (p == m.end())
            return;
        auto& messages = p->second;

        for (auto m = messages.begin(); m != messages.end(); ++m) {
            if (m->second.status == MessageStatus::UNKNOWN
                || m->second.status == MessageStatus::IDLE) {
                m->second.status = MessageStatus::SENDING;
                m->second.retried++;
                m->second.last_op = clock::now();
                pending.emplace_back(PendingMsg {m->first, m->second.to, m->second.payloads});
            }
        }
    }
    // avoid locking while calling callback
    for (const auto& p : pending) {
        JAMI_DEBUG("[message {:d}] Retry sending", p.token);
        if (p.payloads.find("application/im-gitmessage-id") == p.payloads.end())
            emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
                account_.getAccountID(),
                "",
                p.to,
                std::to_string(p.token),
                (int) libjami::Account::MessageStates::SENDING);
        account_.sendMessage(p.to, p.payloads, p.token, retryOnTimeout, false, deviceId);
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
            auto emit = m->second.payloads.find("application/im-gitmessage-id")
                        == m->second.payloads.end();
            m->second.status = MessageStatus::CANCELLED;
            if (emit)
                emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
                    account_.getAccountID(),
                    "",
                    m->second.to,
                    std::to_string(t),
                    static_cast<int>(libjami::Account::MessageStates::CANCELLED));
            save_();
            return true;
        }
    }
    return false;
}

void
MessageEngine::onMessageSent(const std::string& peer,
                             MessageToken token,
                             bool ok,
                             const std::string& deviceId)
{
    JAMI_DEBUG("[message {:d}] Message sent: {:s}", token, ok ? "success"sv : "failure"sv);
    std::lock_guard<std::mutex> lock(messagesMutex_);
    auto& m = deviceId.empty() ? messages_ : messagesDevices_;

    auto p = m.find(deviceId.empty() ? peer : deviceId);
    if (p == m.end())
        return;

    auto f = p->second.find(token);
    if (f != p->second.end()) {
        auto emit = f->second.payloads.find("application/im-gitmessage-id")
                    == f->second.payloads.end();
        if (f->second.status == MessageStatus::SENDING) {
            if (ok) {
                f->second.status = MessageStatus::SENT;
                JAMI_DBG() << "[message " << token << "] Status changed to SENT";
                if (emit)
                    emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
                        account_.getAccountID(),
                        "",
                        f->second.to,
                        std::to_string(token),
                        static_cast<int>(libjami::Account::MessageStates::SENT));
                save_();
            } else if (f->second.retried >= MAX_RETRIES) {
                f->second.status = MessageStatus::FAILURE;
                JAMI_DBG() << "[message " << token << "] Status changed to FAILURE";
                if (emit)
                    emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
                        account_.getAccountID(),
                        "",
                        f->second.to,
                        std::to_string(token),
                        static_cast<int>(libjami::Account::MessageStates::FAILURE));
                save_();
            } else {
                f->second.status = MessageStatus::IDLE;
                JAMI_DEBUG("[message {:d}] Status changed to IDLE", token);
            }
        } else {
            JAMI_DEBUG("[message {:d}] State is not SENDING", token);
        }
    } else {
        JAMI_DEBUG("[message {:d}] Can't find message", token);
    }
}

void
MessageEngine::onMessageDisplayed(const std::string& peer, MessageToken token, bool displayed)
{
    if (not displayed)
        return;
    JAMI_DBG() << "[message " << token << "] Displayed by peer";
    emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
        account_.getAccountID(),
        "", /* No related conversation */
        peer,
        std::to_string(token),
        static_cast<int>(libjami::Account::MessageStates::DISPLAYED));
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
            fileutils::openStream(file, savePath_);
            if (file.is_open())
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
                MessageToken token = from_hex_string(m.key().asString());
                Message msg;
                msg.status = (MessageStatus) jmsg["status"].asInt();
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
        if (loaded > 0) {
            JAMI_DBG("[Account %s] loaded %lu messages from %s",
                     account_.getAccountID().c_str(),
                     loaded,
                     savePath_.c_str());
        }
    } catch (const std::exception& e) {
        JAMI_DBG("[Account %s] couldn't load messages from %s: %s",
                 account_.getAccountID().c_str(),
                 savePath_.c_str(),
                 e.what());
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
                if (v.status == MessageStatus::FAILURE || v.status == MessageStatus::SENT
                    || v.status == MessageStatus::CANCELLED)
                    continue;
                Json::Value msg;
                msg["status"] = (int) (v.status == MessageStatus::SENDING ? MessageStatus::IDLE
                                                                          : v.status);
                msg["to"] = v.to;
                auto wall_time = std::chrono::system_clock::now()
                                 + std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                     v.last_op - clock::now());
                msg["last_op"] = (Json::Value::Int64) std::chrono::system_clock::to_time_t(
                    wall_time);
                msg["retried"] = v.retried;
                auto& payloads = msg["payload"];
                for (const auto& p : v.payloads)
                    payloads[p.first] = p.second;
                peerRoot[to_hex_string(m.first)] = std::move(msg);
            }
            if (peerRoot.size() == 0)
                continue;
            root[c.first] = std::move(peerRoot);
        }

        // Save asynchronously
        dht::ThreadPool::computation().run([path = savePath_,
                                            root = std::move(root),
                                            accountID = account_.getAccountID()] {
            std::lock_guard<std::mutex> lock(fileutils::getFileLock(path));
            try {
                Json::StreamWriterBuilder wbuilder;
                wbuilder["commentStyle"] = "None";
                wbuilder["indentation"] = "";
                std::unique_ptr<Json::StreamWriter> writer(wbuilder.newStreamWriter());
                std::ofstream file;
                file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                fileutils::openStream(file, path, std::ios::trunc);
                if (file.is_open())
                    writer->write(root, &file);
            } catch (const std::exception& e) {
                JAMI_ERROR("[Account {:s}] Couldn't save messages to {:s}: {:s}",
                           accountID,
                           path,
                           e.what());
            }
            JAMI_DEBUG("[Account {:s}] saved {:d} messages to {:s}", accountID, root.size(), path);
        });
    } catch (const std::exception& e) {
        JAMI_ERR("[Account %s] couldn't save messages to %s: %s",
                 account_.getAccountID().c_str(),
                 savePath_.c_str(),
                 e.what());
    }
}

} // namespace im
} // namespace jami
