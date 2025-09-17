/*
 *  Copyright (C) 2026 Savoir-faire Linux Inc.
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

#include "simclient.h"

#include <cassert>

namespace jami {
namespace test {

void
SimClient::onConversationReady(const std::string& accountId, const std::string& conversationId)
{
    assert(accountId_.empty());
    assert(conversationId_.empty());
    accountId_ = accountId;
    conversationId_ = conversationId;
}

void
SimClient::onSwarmLoaded(uint32_t id,
                         const std::string& accountId,
                         const std::string& conversationId,
                         std::vector<SwarmMessage> messages)
{
    assert(accountId == accountId_);
    assert(conversationId == conversationId_);

    if (latestMessageId_.empty() && !messages.empty()) {
        latestMessageId_ = messages.front().id;
    }

    for (const auto& msg : messages) {
        if (indexFromMessageId_.find(msg.id) == indexFromMessageId_.end()) {
            indexFromMessageId_[msg.id] = swarmMessages_.size();
            swarmMessages_.push_back(msg);
        }
    }
}

void
SimClient::onSwarmMessageReceived(const std::string& accountId,
                                  const std::string& conversationId,
                                  const SwarmMessage& message)
{
    assert(accountId == accountId_);
    assert(conversationId == conversationId_);
    assert(indexFromMessageId_.find(message.id) == indexFromMessageId_.end());

    indexFromMessageId_[message.id] = swarmMessages_.size();
    swarmMessages_.push_back(message);

    if (latestMessageId_ == message.linearizedParent) {
        latestMessageId_ = message.id;
    }
}

void
SimClient::onSwarmMessageUpdated(const std::string& accountId,
                                 const std::string& conversationId,
                                 const SwarmMessage& message)
{
    assert(accountId == accountId_);
    assert(conversationId == conversationId_);

    auto it = indexFromMessageId_.find(message.id);
    if (it != indexFromMessageId_.end()) {
        swarmMessages_[it->second] = message;
    }
}

std::vector<libjami::SwarmMessage>
SimClient::getMessages() const
{
    std::vector<SwarmMessage> sortedMessages;
    sortedMessages.reserve(swarmMessages_.size());

    std::string currentMessageId = latestMessageId_;
    ssize_t maxIterations = swarmMessages_.size() + 1;
    while (maxIterations--) {
        auto it = indexFromMessageId_.find(currentMessageId);
        if (it == indexFromMessageId_.end()) {
            break;
        }
        const auto& message = swarmMessages_[it->second];
        sortedMessages.push_back(message);
        currentMessageId = message.linearizedParent;
    }
    assert(maxIterations >= 0 && "Cycle detected in message history");

    return sortedMessages;
}

void
SimClient::clearMessages()
{
    swarmMessages_.clear();
    indexFromMessageId_.clear();
    latestMessageId_.clear();
}

} // namespace test
} // namespace jami
