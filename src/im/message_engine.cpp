/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#include "client/jami_signal.h"
#include "jami/account_const.h"

#include <dhtnet/fileutils.h>
#include <opendht/thread_pool.h>
#include <fmt/std.h>

#include <cstring>
#include <fstream>
#include <vector>

namespace jami {
namespace im {

MessageEngine::MessageEngine(SIPAccountBase& acc, const std::filesystem::path& path)
    : account_(acc)
    , savePath_(path)
    , ioContext_(Manager::instance().ioContext())
    , saveTimer_(*ioContext_)
    , retryTimer_(*ioContext_)
{
    dhtnet::fileutils::check_dir(savePath_.parent_path());
    load();
}

MessageToken
MessageEngine::sendMessage(const std::string& to,
                           const std::string& deviceId,
                           const std::map<std::string, std::string>& payloads,
                           uint64_t refreshToken,
                           std::optional<MessageDelivery> delivery)
{
    if (payloads.empty() or to.empty())
        return 0;
    MessageToken token = 0;
    {
        std::lock_guard lock(messagesMutex_);
        auto newToken = [&] {
            return std::uniform_int_distribution<MessageToken> {1, JAMI_ID_MAX_VAL}(account_.rand);
        };
        auto& peerMessages = deviceId.empty() ? messages_[to] : messagesDevices_[deviceId];
        if (refreshToken != 0) {
            for (auto& m : peerMessages) {
                if (m.token == refreshToken) {
                    token = m.status == MessageStatus::SENDING ? newToken() : refreshToken;
                    m.token = token;
                    m.to = to;
                    m.payloads = payloads;
                    m.delivery = delivery;
                    m.status = MessageStatus::IDLE;
                    m.retried = 0;
                    m.last_op = {};
                    break;
                }
            }
        }
        if (token == 0 && delivery && delivery->completion == MessageCompletion::FETCHED) {
            auto existing = std::find_if(peerMessages.begin(), peerMessages.end(), [&](const auto& message) {
                return message.delivery && message.delivery->completion == MessageCompletion::FETCHED
                       && message.delivery->conversationId == delivery->conversationId;
            });
            if (existing != peerMessages.end()) {
                token = existing->status == MessageStatus::SENDING ? newToken() : existing->token;
                existing->token = token;
                existing->to = to;
                existing->payloads = payloads;
                existing->delivery = delivery;
                existing->status = MessageStatus::IDLE;
                existing->retried = 0;
                existing->last_op = {};
            }
        }
        if (token == 0) {
            token = newToken();
            Message message;
            message.token = token;
            auto& m = peerMessages.emplace_back(std::move(message));
            m.to = to;
            m.payloads = payloads;
            m.delivery = std::move(delivery);
        }
        scheduleFetchedRetry();
        scheduleSave();
    }
    asio::post(*ioContext_, [this, w = account_.weak_from_this(), to, deviceId]() {
        if (w.lock())
            retrySend(to, deviceId, true);
    });
    return token;
}

void
MessageEngine::onPeerOnline(const std::string& peer, const std::string& deviceId, bool retryOnTimeout)
{
    retrySend(peer, deviceId, retryOnTimeout);
}

void
MessageEngine::onRegistrationResumed()
{
    std::vector<std::string> peers;
    std::vector<std::string> devices;
    {
        std::lock_guard lock(messagesMutex_);
        peers.reserve(messages_.size());
        for (const auto& [peer, _] : messages_)
            peers.emplace_back(peer);
        devices.reserve(messagesDevices_.size());
        for (const auto& [device, _] : messagesDevices_)
            devices.emplace_back(device);
    }

    auto w = account_.weak_from_this();
    for (auto& peer : peers) {
        asio::post(*ioContext_, [this, w, peer = std::move(peer)] {
            if (w.lock())
                retrySend(peer, {}, true);
        });
    }
    for (auto& device : devices) {
        asio::post(*ioContext_, [this, w, device = std::move(device)] {
            if (w.lock())
                retrySend({}, device, true);
        });
    }
}

void
MessageEngine::acknowledgeDeviceFetched(const std::string& conversationId,
                                        const std::string& deviceId,
                                        const std::string& commitId,
                                        const CommitCovered& commitCovered)
{
    std::set<std::string> candidates;
    {
        std::lock_guard lock(messagesMutex_);
        auto queue = messagesDevices_.find(deviceId);
        if (queue == messagesDevices_.end())
            return;
        for (const auto& message : queue->second)
            if (message.delivery && message.delivery->completion == MessageCompletion::FETCHED
                && message.delivery->conversationId == conversationId)
                candidates.emplace(message.delivery->commitId);
    }

    std::set<std::string> acknowledged;
    for (const auto& advertised : candidates)
        if (commitCovered(advertised, commitId))
            acknowledged.emplace(advertised);
    if (acknowledged.empty())
        return;

    std::lock_guard lock(messagesMutex_);
    auto queue = messagesDevices_.find(deviceId);
    if (queue == messagesDevices_.end())
        return;
    const auto removed = std::erase_if(queue->second, [&](const auto& message) {
        return message.delivery && message.delivery->completion == MessageCompletion::FETCHED
               && message.delivery->conversationId == conversationId
               && acknowledged.contains(message.delivery->commitId);
    });
    if (queue->second.empty())
        messagesDevices_.erase(queue);
    if (removed) {
        scheduleFetchedRetry();
        scheduleSave();
    }
}

void
MessageEngine::acknowledgeMemberFetched(const std::string& conversationId,
                                        const std::string& memberUri,
                                        const std::string& commitId,
                                        const CommitCovered& commitCovered)
{
    std::set<std::string> candidates;
    {
        std::lock_guard lock(messagesMutex_);
        auto collect = [&](const auto& queues) {
            for (const auto& [_, messages] : queues)
                for (const auto& message : messages)
                    if (message.to == memberUri && message.delivery
                        && message.delivery->completion == MessageCompletion::FETCHED
                        && message.delivery->conversationId == conversationId)
                        candidates.emplace(message.delivery->commitId);
        };
        collect(messages_);
        collect(messagesDevices_);
    }

    std::set<std::string> acknowledged;
    for (const auto& advertised : candidates)
        if (commitCovered(advertised, commitId))
            acknowledged.emplace(advertised);
    if (acknowledged.empty())
        return;

    std::lock_guard lock(messagesMutex_);
    size_t removed = 0;
    auto eraseAcknowledged = [&](auto& queues) {
        for (auto queue = queues.begin(); queue != queues.end();) {
            removed += std::erase_if(queue->second, [&](const auto& message) {
                  return message.to == memberUri && message.delivery
                       && message.delivery->completion == MessageCompletion::FETCHED
                       && message.delivery->conversationId == conversationId
                       && acknowledged.contains(message.delivery->commitId);
            });
            if (queue->second.empty())
                queue = queues.erase(queue);
            else
                ++queue;
        }
    };
    eraseAcknowledged(messages_);
    eraseAcknowledged(messagesDevices_);
    if (removed) {
        scheduleFetchedRetry();
        scheduleSave();
    }
}

#ifdef LIBJAMI_TEST
size_t
MessageEngine::pendingMessageCount() const
{
    std::lock_guard lock(messagesMutex_);
    size_t count = 0;
    for (const auto& [_, messages] : messages_)
        count += messages.size();
    for (const auto& [_, messages] : messagesDevices_)
        count += messages.size();
    return count;
}
#endif

void
MessageEngine::retrySend(const std::string& peer, const std::string& deviceId, bool retryOnTimeout)
{
    struct PendingMsg
    {
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

        for (auto& m : messages) {
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
MessageEngine::onMessageSent(const std::string& peer, MessageToken token, bool ok, const std::string& deviceId)
{
    JAMI_DEBUG("[Account {:s}] [message {:d}] Message sent: {:s}",
               account_.getAccountID(),
               token,
               ok ? "success"sv : "failure"sv);
    std::lock_guard lock(messagesMutex_);
    auto& m = deviceId.empty() ? messages_ : messagesDevices_;

    auto p = m.find(deviceId.empty() ? peer : deviceId);
    if (p == m.end()) {
        JAMI_WARNING("[Account {:s}] onMessageSent: Peer not found: id:{} device:{}",
                     account_.getAccountID(),
                     peer,
                     deviceId);
        return;
    }

    auto f = std::find_if(p->second.begin(), p->second.end(), [&](const Message& m) { return m.token == token; });
    if (f != p->second.end()) {
        auto emit = f->payloads.find("application/im-gitmessage-id") == f->payloads.end();
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
                if (!f->delivery || f->delivery->completion == MessageCompletion::WRITE)
                    p->second.erase(f);
                scheduleFetchedRetry();
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
                scheduleFetchedRetry();
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
        decltype(messages_) messages;
        decltype(messagesDevices_) deviceMessages;
        {
            std::lock_guard lock(dhtnet::fileutils::getFileLock(savePath_));
            const auto data = fileutils::loadFile(savePath_);
            msgpack::unpacker unpacker;
            unpacker.reserve_buffer(data.size());
            std::memcpy(unpacker.buffer(), data.data(), data.size());
            unpacker.buffer_consumed(data.size());

            msgpack::object_handle object;
            if (!unpacker.next(object))
                throw std::runtime_error("Invalid message queue");
            object.get().convert(messages);
            if (unpacker.next(object))
                object.get().convert(deviceMessages);
        }
        std::lock_guard lock(messagesMutex_);
        messages_ = std::move(messages);
        messagesDevices_ = std::move(deviceMessages);
        normalizeLoadedMessages();
        if (not messages_.empty() || not messagesDevices_.empty())
            JAMI_LOG("[Account {}] Loaded {} peer and {} device message queues from {}",
                     account_.getAccountID(),
                     messages_.size(),
                     messagesDevices_.size(),
                     savePath_);
    } catch (const std::exception& e) {
        JAMI_LOG("[Account {}] Unable to load messages from {}: {}", account_.getAccountID(), savePath_, e.what());
    }
}

void
MessageEngine::normalizeLoadedMessages()
{
    auto normalize = [](auto& queues) {
        for (auto& [_, messages] : queues)
            for (auto& message : messages)
                if (message.status == MessageStatus::SENDING
                    || (message.status == MessageStatus::SENT && message.delivery
                        && message.delivery->completion == MessageCompletion::FETCHED))
                    message.status = MessageStatus::IDLE;
    };
    normalize(messages_);
    normalize(messagesDevices_);
}

void
MessageEngine::scheduleFetchedRetry()
{
    std::optional<clock::time_point> nextRetry;
    auto findNext = [&](const auto& queues) {
        for (const auto& [_, messages] : queues)
            for (const auto& message : messages)
                if (message.status == MessageStatus::SENT && message.delivery
                    && message.delivery->completion == MessageCompletion::FETCHED) {
                    auto retry = message.last_op + FETCH_RETRY_DELAY;
                    if (!nextRetry || retry < *nextRetry)
                        nextRetry = retry;
                }
    };
    findNext(messages_);
    findNext(messagesDevices_);
    if (!nextRetry) {
        retryTimer_.cancel();
        return;
    }

    auto delay = *nextRetry - clock::now();
    retryTimer_.expires_after(std::max(delay, clock::duration::zero()));
    retryTimer_.async_wait([this, w = account_.weak_from_this()](const std::error_code& ec) {
        if (!ec && w.lock())
            retryFetched();
    });
}

void
MessageEngine::retryFetched()
{
    std::vector<std::string> peers;
    std::vector<std::string> devices;
    {
        std::lock_guard lock(messagesMutex_);
        const auto now = clock::now();
        auto activate = [&](auto& queues, auto& ready) {
            for (auto& [target, messages] : queues) {
                bool targetReady = false;
                for (auto& message : messages)
                    if (message.status == MessageStatus::SENT && message.delivery
                        && message.delivery->completion == MessageCompletion::FETCHED
                        && message.last_op + FETCH_RETRY_DELAY <= now) {
                        message.status = MessageStatus::IDLE;
                        targetReady = true;
                    }
                if (targetReady)
                    ready.emplace_back(target);
            }
        };
        activate(messages_, peers);
        activate(messagesDevices_, devices);
        scheduleFetchedRetry();
        if (!peers.empty() || !devices.empty())
            scheduleSave();
    }

    for (const auto& peer : peers)
        retrySend(peer, {}, true);
    for (const auto& device : devices)
        retrySend({}, device, true);
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
        file.open(savePath_, std::ios::trunc | std::ios::binary);
        if (file.is_open()) {
            msgpack::pack(file, messages_);
            msgpack::pack(file, messagesDevices_);
        }
    } catch (const std::exception& e) {
        JAMI_ERROR("[Account {}] Unable to serialize pending messages: {}", account_.getAccountID(), e.what());
    }
}

} // namespace im
} // namespace jami
