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

#include "jamidht/commit_message.h"

#undef NDEBUG
#include <cassert>
#include <algorithm>
#include <random>

namespace jami {
namespace test {

void
SimClient::onConversationMemberEvent(const std::string& accountId,
                                     const std::string& conversationId,
                                     const std::string& memberId,
                                     int eventCode)
{
    using libjami::MemberEvent;

    assert(accountId == accountId_);
    assert(conversationId == conversationId_);

    auto event = static_cast<MemberEvent>(eventCode);
    if (event != MemberEvent::ADD) {
        assert(memberRole_.contains(memberId));
    }

    switch (event) {
    case MemberEvent::ADD:
        memberRole_[memberId] = MemberRole::INVITED;
        break;
    case MemberEvent::JOIN:
        memberRole_[memberId] = MemberRole::MEMBER;
        break;
    case MemberEvent::UNBAN:
        memberRole_[memberId] = MemberRole::MEMBER;
        break;
    case MemberEvent::REMOVE:
        memberRole_[memberId] = MemberRole::LEFT;
        break;
    case MemberEvent::BAN:
        memberRole_[memberId] = MemberRole::BANNED;
        break;
    default:
        assert(false && "Invalid member event received, this is a bug!");
        break;
    }
}

void
SimClient::onConversationReady(const std::string& accountId, const std::string& conversationId)
{
    assert(accountId_.empty());
    assert(conversationId_.empty());
    accountId_ = accountId;
    conversationId_ = conversationId;
}

void
SimClient::setMemberRoles(const std::vector<std::map<std::string, std::string>>& members)
{
    assert(memberRole_.empty());
    for (const auto& member : members) {
        auto memberId = member.at("uri");
        assert(!memberId.empty());

        auto roleStr = member.at("role");
        MemberRole role;
        if (roleStr == "admin") {
            assert(adminId_.empty());
            adminId_ = memberId;
            role = MemberRole::ADMIN;
        } else if (roleStr == "member") {
            role = MemberRole::MEMBER;
        } else if (roleStr == "invited") {
            role = MemberRole::INVITED;
        } else if (roleStr == "banned") {
            role = MemberRole::BANNED;
        } else if (roleStr == "left") {
            role = MemberRole::LEFT; // For one to one
        } else {
            assert(false && "Invalid role received");
        }
        memberRole_[memberId] = role;
    }
    assert(!adminId_.empty());
}

void
SimClient::onSwarmLoaded(uint32_t /* id */,
                         const std::string& accountId,
                         const std::string& conversationId,
                         const std::vector<SwarmMessage>& messages)
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
    if (auto replyToIt = message.body.find(CommitKey::REPLY_TO); replyToIt != message.body.end()) {
        assert(!replyToIt->second.empty());
        assert(indexFromMessageId_.contains(replyToIt->second));
    }

    insertMessage(message);
}

void
SimClient::onSwarmMessageUpdated(const std::string& accountId,
                                 const std::string& conversationId,
                                 const SwarmMessage& message)
{
    assert(accountId == accountId_);
    assert(conversationId == conversationId_);

    auto indexIt = indexFromMessageId_.find(message.id);
    assert(indexIt != indexFromMessageId_.end());
    auto index = indexIt->second;
    swarmMessages_[index] = message;

    auto it = std::find(sortedIndices_.begin(), sortedIndices_.end(), index);
    assert(it != sortedIndices_.end());
    sortedIndices_.erase(it);

    auto parentId = message.linearizedParent;
    assert(!parentId.empty());
    auto parentIndexIt = indexFromMessageId_.find(parentId);
    assert(parentIndexIt != indexFromMessageId_.end());
    auto parentIndex = parentIndexIt->second;
    auto parentIt = std::find(sortedIndices_.begin(), sortedIndices_.end(), parentIndex);
    assert(parentIt != sortedIndices_.end());
    sortedIndices_.insert(parentIt + 1, index);
}

void
SimClient::onReactionAdded(const std::string& accountId,
                           const std::string& conversationId,
                           const std::string& messageId,
                           const std::map<std::string, std::string>& reaction)
{
    assert(accountId == accountId_);
    assert(conversationId == conversationId_);

    // Reactions must not enter the message list, only update the target message's reactions.
    auto indexIt = indexFromMessageId_.find(messageId);
    assert(indexIt != indexFromMessageId_.end());
    auto index = indexIt->second;
    auto& reactions = swarmMessages_[index].reactions;

    auto reactionIdIt = reaction.find("id");
    assert(reactionIdIt != reaction.end());
    const auto& reactionId = reactionIdIt->second;
    for (const auto& r : reactions) {
        auto idIt = r.find("id");
        assert(idIt != r.end());
        assert(idIt->second != reactionId);
    }

    reactions.emplace_back(reaction);
}

void
SimClient::onReactionRemoved(const std::string& accountId,
                             const std::string& conversationId,
                             const std::string& messageId,
                             const std::string& reactionId)
{
    assert(accountId == accountId_);
    assert(conversationId == conversationId_);

    auto indexIt = indexFromMessageId_.find(messageId);
    assert(indexIt != indexFromMessageId_.end());
    auto index = indexIt->second;
    auto& reactions = swarmMessages_[index].reactions;

    auto it = std::find_if(reactions.begin(), reactions.end(), [&](const auto& r) {
        auto idIt = r.find("id");
        assert(idIt != r.end());
        return idIt->second == reactionId;
    });
    assert(it != reactions.end());
    reactions.erase(it);
}

void
SimClient::onActiveCallsChanged(const std::string& accountId,
                                const std::string& conversationId,
                                const std::vector<std::map<std::string, std::string>>& activeCalls)
{
    assert(accountId == accountId_);
    assert(conversationId == conversationId_);
    activeCalls_ = activeCalls;
}

MemberRole
SimClient::getMemberRole(const std::string& memberId) const
{
    auto it = memberRole_.find(memberId);
    if (it == memberRole_.end()) {
        return MemberRole::INVALID;
    }
    return it->second;
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

const std::vector<std::map<std::string, std::string>>&
SimClient::getActiveCalls() const
{
    return activeCalls_;
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

int
SimClient::getIndex(const std::string& messageId) const
{
    return static_cast<int>(indexFromMessageId_.at(messageId));
}

const libjami::SwarmMessage&
SimClient::getMessageAtIndex(int index) const
{
    assert(index >= 0 && index < static_cast<int>(swarmMessages_.size()));
    return swarmMessages_[index];
}

int
SimClient::randomMessageIndex(std::mt19937_64& gen) const
{
    if (swarmMessages_.empty())
        return -1;
    std::uniform_int_distribution<size_t> dist(0, swarmMessages_.size() - 1);
    return static_cast<int>(dist(gen));
}

std::string
SimClient::reactionByAuthor(int messageIndex, const std::string& authorUri) const
{
    const auto& reactions = getMessageAtIndex(messageIndex).reactions;
    for (const auto& reaction : reactions) {
        auto authorIt = reaction.find("author");
        if (authorIt != reaction.end() && authorIt->second == authorUri) {
            return reaction.at("id");
        }
    }
    return {};
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
        assert(message.type == CommitType::INITIAL);
        sortedIndices_.insert(sortedIndices_.begin(), index);
    }
}
} // namespace test
} // namespace jami
