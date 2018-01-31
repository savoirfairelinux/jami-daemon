/*
 *  Copyright (C) 2016-2018 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include <chrono>
#include <mutex>
#include <sstream>

namespace ring {
namespace im {

static constexpr auto schedule_period = std::chrono::minutes(1);
static constexpr unsigned max_retries = 3;

namespace
{

class Message {
public:
    Message(MessageToken token, const std::string& recipient, const std::map<std::string, std::string>& contents)
        : token {token}, recipient {recipient}, contents {contents} {}

    const MessageToken token;
    const std::string recipient;
    const std::map<std::string, std::string> contents;
    MessageStatus status {MessageStatus::CREATED};
    unsigned retries {0};
};

static inline std::ostream& operator<< (std::ostream& os, const Message& msg)
{
    os << "[message " << std::hex << msg.token << std::dec << "] ";
    return os;
}

}

class MessageEngine::Impl
{
public:

    using clock = std::chrono::steady_clock;

    explicit Impl(SIPAccountBase& acc) : account {acc} {}

    MessageToken sendNewMessage(const std::string&, const std::map<std::string, std::string>&);
    void emitMessageChange(const Message&, DRing::Account::MessageStates);
    void onMessageSent(MessageToken, bool);

    SIPAccountBase& account;
    mutable std::mutex messageMapMutex;
    std::map<MessageToken, Message> messageMap;
    std::uniform_int_distribution<MessageToken> udist {1};

private:
    NON_COPYABLE(Impl);
    MessageToken generateToken();
    void tryToSendMsg(Message&);
    void reschedule(MessageToken);
};

inline MessageToken
MessageEngine::Impl::generateToken()
{
    MessageToken token;
    do {
        token = udist(account.rand);
    } while (messageMap.find(token) != std::cend(messageMap));
    return token;
}

inline void
MessageEngine::Impl::emitMessageChange(const Message& msg, DRing::Account::MessageStates status)
{
    emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(
        account.getAccountID(), msg.token, msg.recipient, int(status));
}

inline void
MessageEngine::Impl::tryToSendMsg(Message& msg)
{
    if (msg.status > MessageStatus::SENDING)
        return;

    if (msg.retries++ >= max_retries) {
        RING_ERR() << msg << "max retries reached";
        msg.status = MessageStatus::FAILURE;
        emitMessageChange(msg, DRing::Account::MessageStates::FAILURE);
        return;
    }

    RING_DBG() << msg << "sending, try #" << msg.retries;
    msg.status = MessageStatus::SENDING;
    emitMessageChange(msg, DRing::Account::MessageStates::SENDING);
    account.sendTextMessage(msg.recipient, msg.contents, msg.token);
    reschedule(msg.token);
}

void
MessageEngine::Impl::reschedule(MessageToken token)
{
    auto when = clock::now() + schedule_period;
    std::weak_ptr<SIPAccountBase> account_locker = std::static_pointer_cast<SIPAccountBase>(account.shared_from_this());
    Manager::instance().scheduleTask(
        [this, token, account_locker] {
            if (auto account = account_locker.lock()) {
                std::unique_lock<std::mutex> lk {messageMapMutex};
                const auto& iter = messageMap.find(token);
                if (iter == std::cend(messageMap))
                    return;
                lk.unlock();
                tryToSendMsg(iter->second);
            }
        }, when);
}

MessageToken
MessageEngine::Impl::sendNewMessage(const std::string& recipient,
                                    const std::map<std::string, std::string>& contents)
{
    std::unique_lock<std::mutex> lk {messageMapMutex};
    auto token = generateToken();
    auto result = messageMap.emplace(token, Message(token, recipient, contents));
    lk.unlock();
    tryToSendMsg(result.first->second);
    return token;
}

void
MessageEngine::Impl::onMessageSent(MessageToken token, bool ok)
{
    std::unique_lock<std::mutex> lk {messageMapMutex};
    const auto& iter = messageMap.find(token);
    if (iter == std::cend(messageMap)) {
        auto status = ok ? DRing::Account::MessageStates::SENT : DRing::Account::MessageStates::FAILURE;
        RING_WARN() << "status " << (ok ? "success" : "failure")
                    << " from unknown message " << std::hex << token << std::dec;
        lk.unlock();
        emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(
            account.getAccountID(), token, "", int(status));
        return;
    }

    auto& msg = iter->second;
    if (msg.status == MessageStatus::SENDING) {
        if (ok) {
            msg.status = MessageStatus::SENT;
            lk.unlock();
            RING_DBG() << msg << "sending done";
            emitMessageChange(msg, DRing::Account::MessageStates::SENT);
        } else {
            msg.status = MessageStatus::FAILURE;
            lk.unlock();
            RING_DBG() << msg << "sending error";
            emitMessageChange(msg, DRing::Account::MessageStates::FAILURE);
        }
    } else {
        RING_ERR() << msg << "received before being sent";
    }
}

//==============================================================================

MessageEngine::MessageEngine(SIPAccountBase& account)
    : pimpl_ { std::make_unique<Impl>(account) }
{}

MessageEngine::~MessageEngine() = default;

MessageToken
MessageEngine::sendMessage(const std::string& recipient,
                            const std::map<std::string, std::string>& contents)
{
    if (recipient.empty() or contents.empty())
        return 0;
    return pimpl_->sendNewMessage(recipient, contents);
}

MessageStatus
MessageEngine::getStatus(MessageToken token) const
{
    std::lock_guard<std::mutex> lk {pimpl_->messageMapMutex};
    const auto& iter = pimpl_->messageMap.find(token);
    if (iter == std::cend(pimpl_->messageMap))
        return MessageStatus::UNKNOWN;
    return iter->second.status;
}

void
MessageEngine::onMessageSent(MessageToken token, bool ok)
{
    pimpl_->onMessageSent(token, ok);
}

}} // namespace ring::im
