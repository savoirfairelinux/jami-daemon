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

#undef NDEBUG
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

    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        insertMessage(*it);
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

    insertMessage(message);
}

void
SimClient::onSwarmMessageUpdated(const std::string& accountId,
                                 const std::string& conversationId,
                                 const SwarmMessage& message)
{
    assert(accountId == accountId_);
    assert(conversationId == conversationId_);

    auto index = indexFromMessageId_.at(message.id);
    swarmMessages_[index] = message;

    auto it = std::find(sortedIndices_.begin(), sortedIndices_.end(), index);
    assert(it != sortedIndices_.end());
    sortedIndices_.erase(it);

    auto parentId = message.linearizedParent;
    assert(!parentId.empty());
    auto parentIndex = indexFromMessageId_.at(parentId);
    auto parentIt = std::find(sortedIndices_.begin(), sortedIndices_.end(), parentIndex);
    assert(parentIt != sortedIndices_.end());
    sortedIndices_.insert(parentIt + 1, index);
}

std::vector<libjami::SwarmMessage>
SimClient::getMessages() const
{
    std::vector<SwarmMessage> sortedMessages;
    sortedMessages.reserve(swarmMessages_.size());

    for (auto it = sortedIndices_.rbegin(); it != sortedIndices_.rend(); it++) {
        sortedMessages.push_back(swarmMessages_[*it]);
    }
    return sortedMessages;
}

void
SimClient::clearMessages()
{
    swarmMessages_.clear();
    sortedIndices_.clear();
    indexFromMessageId_.clear();
}

bool
SimClient::hasConsistentHistory() const
{
    for (size_t i = 0; i + 1 < sortedIndices_.size(); i++) {
        const auto& parent = swarmMessages_[sortedIndices_[i]];
        const auto& child = swarmMessages_[sortedIndices_[i + 1]];
        if (child.linearizedParent != parent.id) {
            return false;
        }
    }
    return true;
}

void
SimClient::insertMessage(const SwarmMessage& message)
{
    if (indexFromMessageId_.contains(message.id)) {
        return;
    }

    auto index = swarmMessages_.size();
    indexFromMessageId_[message.id] = index;
    swarmMessages_.push_back(message);

    if (!message.linearizedParent.empty()) {
        auto parentIndex = indexFromMessageId_.at(message.linearizedParent);
        auto it = std::find(sortedIndices_.begin(), sortedIndices_.end(), parentIndex);
        assert(it != sortedIndices_.end());
        sortedIndices_.insert(it + 1, index);
    } else {
        assert(message.type == "initial");
        sortedIndices_.insert(sortedIndices_.begin(), index);
    }
}
} // namespace test
} // namespace jami
