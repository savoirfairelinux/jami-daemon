/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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

#include "message_engine.h"
#include "sip/sipaccountbase.h"
#include "manager.h"
#include "fileutils.h"

#include "client/ring_signal.h"
#include "jami/account_const.h"

#include <opendht/thread_pool.h>
#include <fmt/std.h>

#include <fstream>

namespace jami {
namespace im {

MessageEngine::MessageEngine(SIPAccountBase& acc, const std::filesystem::path& path)
    : account_(acc)
    , savePath_(path)
    , ioContext_(Manager::instance().ioContext())
    , saveTimer_(*ioContext_)
{
    dhtnet::fileutils::check_dir(savePath_.parent_path());
}

MessageToken
MessageEngine::sendMessage(const std::string& to,
                           const std::string& deviceId,
                           const std::map<std::string, std::string>& payloads,
                           uint64_t refreshToken)
{
    if (payloads.empty() or to.empty())
        return 0;
    MessageToken token = 0;
    {
        std::lock_guard lock(messagesMutex_);
        auto& peerMessages = deviceId.empty() ? messages_[to] : messagesDevices_[deviceId];
        if (refreshToken != 0) {
            for (auto& m : peerMessages) {
                if (m.token == refreshToken) {
                    token = refreshToken;
                    m.to = to;
                    m.payloads = payloads;
                    m.status = MessageStatus::IDLE;
                    break;
                }
            }
        }
        if (token == 0) {
            token = std::uniform_int_distribution<MessageToken> {1, JAMI_ID_MAX_VAL}(account_.rand);
            auto& m = peerMessages.emplace_back(Message {token});
            m.to = to;
            m.payloads = payloads;
        }
        scheduleSave();
    }
    ioContext_->post([this, to, deviceId]() { retrySend(to, deviceId, true); });
    return token;
}

void
MessageEngine::onPeerOnline(const std::string& peer,
                            const std::string& deviceId,
                            bool retryOnTimeout)
{
    retrySend(peer, deviceId, retryOnTimeout);
}

void
MessageEngine::retrySend(const std::string& peer, const std::string& deviceId, bool retryOnTimeout)
{
    struct PendingMsg {
        MessageToken token;
        std::string to;
        std::map<std::string, std::string> payloads;
    };
    std::vector<PendingMsg> pending {};
    auto now = clock::now();
    {
        std::lock_guard lock(messagesMutex_);
        auto& m = deviceId.empty() ? messages_ : messagesDevices_;
        auto p = m.find(deviceId.empty() ? peer : deviceId);
        if (p == m.end())
            return;
        auto& messages = p->second;

        for (auto& m: messages) {
            if (m.status == MessageStatus::IDLE) {
                m.status = MessageStatus::SENDING;
                m.retried++;
                m.last_op = now;
                pending.emplace_back(PendingMsg {m.token, m.to, m.payloads});
            }
        }
    }
    // avoid locking while calling callback
    for (const auto& p : pending) {
        JAMI_DEBUG("[Account {:s}] [message {:d}] Reattempt sending", account_.getAccountID(), p.token);
        if (p.payloads.find("application/im-gitmessage-id") == p.payloads.end())
            emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
                account_.getAccountID(),
                "",
                p.to,
                std::to_string(p.token),
                (int) libjami::Account::MessageStates::SENDING);
        account_.sendMessage(p.to, deviceId, p.payloads, p.token, retryOnTimeout, false);
    }
}

MessageStatus
MessageEngine::getStatus(MessageToken t) const
{
    std::lock_guard lock(messagesMutex_);
    for (const auto& p : messages_) {
        for (const auto& m : p.second) {
            if (m.token == t)
                return m.status;
        }
    }
    return MessageStatus::UNKNOWN;
}

void
MessageEngine::onMessageSent(const std::string& peer,
                             MessageToken token,
                             bool ok,
                             const std::string& deviceId)
{
    JAMI_DEBUG("[Account {:s}] [message {:d}] Message sent: {:s}", account_.getAccountID(), token, ok ? "success"sv : "failure"sv);
    std::lock_guard lock(messagesMutex_);
    auto& m = deviceId.empty() ? messages_ : messagesDevices_;

    auto p = m.find(deviceId.empty() ? peer : deviceId);
    if (p == m.end()) {
        JAMI_WARNING("[Account {:s}] onMessageSent: Peer not found: id:{} device:{}", account_.getAccountID(), peer, deviceId);
        return;
    }

    auto f = std::find_if(p->second.begin(), p->second.end(), [&](const Message& m) {
        return m.token == token;
    });
    if (f != p->second.end()) {
        auto emit = f->payloads.find("application/im-gitmessage-id")
                    == f->payloads.end();
        if (f->status == MessageStatus::SENDING) {
            if (ok) {
                f->status = MessageStatus::SENT;
                JAMI_LOG("[Account {:s}] [message {:d}] Status changed to SENT", account_.getAccountID(), token);
                if (emit)
                    emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
                        account_.getAccountID(),
                        "",
                        f->to,
                        std::to_string(token),
                        static_cast<int>(libjami::Account::MessageStates::SENT));
                p->second.erase(f);
                scheduleSave();
            } else if (f->retried >= MAX_RETRIES) {
                f->status = MessageStatus::FAILURE;
                JAMI_WARNING("[Account {:s}] [message {:d}] Status changed to FAILURE", account_.getAccountID(), token);
                if (emit)
                    emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
                        account_.getAccountID(),
                        "",
                        f->to,
                        std::to_string(token),
                        static_cast<int>(libjami::Account::MessageStates::FAILURE));
                p->second.erase(f);
                scheduleSave();
            } else {
                f->status = MessageStatus::IDLE;
                JAMI_DEBUG("[Account {:s}] [message {:d}] Status changed to IDLE", account_.getAccountID(), token);
            }
        } else {
            JAMI_DEBUG("[Account {:s}] [message {:d}] State is not SENDING", account_.getAccountID(), token);
        }
    } else {
        JAMI_DEBUG("[Account {:s}] [message {:d}] Unable to find message", account_.getAccountID(), token);
    }
}

void
MessageEngine::load()
{
    try {
        decltype(messages_) root;
        {
            std::lock_guard lock(dhtnet::fileutils::getFileLock(savePath_));
            std::ifstream file;
            file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
            file.open(savePath_);
            if (file.is_open()) {
                msgpack::unpacker up;
                up.reserve_buffer(UINT16_MAX);
                while (file.read(up.buffer(), UINT16_MAX)) {
                    up.buffer_consumed(file.gcount());
                    msgpack::object_handle oh;
                    if (up.next(oh)) {
                        root = oh.get().as<std::map<std::string, std::list<Message>>>();
                        break;
                    }
                    up.reserve_buffer(UINT16_MAX);
                }
            }
        }
        std::lock_guard lock(messagesMutex_);
        messages_ = std::move(root);
        if (not messages_.empty()) {
            JAMI_LOG("[Account {}] Loaded {} messages from {}",
                     account_.getAccountID(),
                     messages_.size(),
                     savePath_);
        }
    } catch (const std::exception& e) {
        JAMI_LOG("[Account {}] Unable to load messages from {}: {}",
                 account_.getAccountID(),
                 savePath_,
                 e.what());
    }
}

void
MessageEngine::save() const
{
    std::lock_guard lock(messagesMutex_);
    save_();
}

void
MessageEngine::scheduleSave()
{
    saveTimer_.expires_after(std::chrono::seconds(5));
    saveTimer_.async_wait([this, w = account_.weak_from_this()](const std::error_code& ec) {
        if (!ec)
            if (auto acc = w.lock())
                save();
    });
}

void
MessageEngine::save_() const
{
    try {
        std::ofstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(savePath_, std::ios::trunc);
        if (file.is_open())
            msgpack::pack(file, messages_);
    } catch (const std::exception& e) {
        JAMI_ERROR("[Account {}] Unable to serialize pending messages: {}",
                 account_.getAccountID(), e.what());
    }
}

} // namespace im
} // namespace jami
