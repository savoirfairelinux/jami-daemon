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
#pragma once

#include "jami/conversation_interface.h"

#include <string>
#include <vector>

namespace jami {
namespace test {

/*
TODO Handle the following signals:
- MessagesFound (for search)
- ReactionAdded
- ReactionRemoved
- ConversationMemberEvent (to keep track of participants)
- OnConversationError
 */

enum class MemberRole { INVALID = -1, ADMIN = 0, MEMBER, INVITED, BANNED, LEFT };

class SimClient
{
    using SwarmMessage = libjami::SwarmMessage;

public:
    void onConversationMemberEvent(const std::string& accountId,
                                   const std::string& conversationId,
                                   const std::string& memberId,
                                   int event);
    void onConversationReady(const std::string& accountId, const std::string& conversationId);
    void onSwarmLoaded(uint32_t id,
                       const std::string& accountId,
                       const std::string& conversationId,
                       const std::vector<SwarmMessage>& messages);
    void onSwarmMessageReceived(const std::string& accountId,
                                const std::string& conversationId,
                                const SwarmMessage& message);
    void onSwarmMessageUpdated(const std::string& accountId,
                               const std::string& conversationId,
                               const SwarmMessage& message);

    void setMemberRoles(const std::vector<std::map<std::string, std::string>>& members);
    MemberRole getMemberRole(const std::string& memberId) const;

    std::vector<SwarmMessage> getMessages() const;
    void clearMessages();
    bool hasConsistentHistory() const;

private:
    void insertMessage(const SwarmMessage& message);

    std::string accountId_;
    std::string conversationId_;

    std::string adminId_;
    std::map<std::string, MemberRole> memberRole_;

    std::vector<SwarmMessage> swarmMessages_;
    std::vector<size_t> sortedIndices_; // From oldest to newest message
    std::map<std::string, size_t> indexFromMessageId_;
};

} // namespace test
} // namespace jami
