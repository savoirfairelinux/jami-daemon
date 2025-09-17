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

#include <cppunit/TestAssert.h>

namespace jami {
namespace test {

void
SimClient::onConversationReady(const std::string& accountId, const std::string& conversationId)
{
    CPPUNIT_ASSERT(accountId_.empty());
    CPPUNIT_ASSERT(conversationId_.empty());
    accountId_ = accountId;
    conversationId_ = conversationId;
}

void
SimClient::onSwarmLoaded(uint32_t id,
                         const std::string& accountId,
                         const std::string& conversationId,
                         std::vector<SwarmMessage> messages)
{
    CPPUNIT_ASSERT(accountId == accountId_);
    CPPUNIT_ASSERT(conversationId == conversationId_);

    // TODO Handle cases where
    // - some messages are already there
    // - latestMessageId_ needs to be changed
    // - the new messages are disconnected from the existing ones
    for (const auto& msg : messages) {
        indexFromMessageId_[msg.id] = swarmMessages_.size();
        swarmMessages_.push_back(msg);
    }
}

void
SimClient::onSwarmMessageReceived(const std::string& accountId,
                                  const std::string& conversationId,
                                  const SwarmMessage& message)
{
    CPPUNIT_ASSERT(accountId == accountId_);
    CPPUNIT_ASSERT(conversationId == conversationId_);
    CPPUNIT_ASSERT(indexFromMessageId_.find(message.id) == indexFromMessageId_.end());

    indexFromMessageId_[message.id] = swarmMessages_.size();
    swarmMessages_.push_back(message);
}

void
SimClient::onSwarmMessageUpdated(const std::string& accountId,
                                 const std::string& conversationId,
                                 const SwarmMessage& message)
{}

} // namespace test
} // namespace jami
