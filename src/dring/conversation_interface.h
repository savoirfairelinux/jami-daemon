/*
 *  Copyright (C) 2013-2019 Savoir-faire Linux Inc.
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#ifndef DRING_CONVERSATIONI_H
#define DRING_CONVERSATIONI_H

#include "def.h"

#include <vector>
#include <map>
#include <string>

#include "dring.h"

namespace DRing {

// Conversation management
DRING_PUBLIC std::string startConversation(const std::string& accountId);
DRING_PUBLIC void acceptConversationRequest(const std::string& accountId,
                                            const std::string& conversationId);
DRING_PUBLIC void declineConversationRequest(const std::string& accountId,
                                             const std::string& conversationId);
DRING_PUBLIC bool removeConversation(const std::string& accountId,
                                     const std::string& conversationId);
DRING_PUBLIC std::vector<std::string> getConversations(const std::string& accountId);
DRING_PUBLIC std::vector<std::map<std::string, std::string>> getConversationRequests(
    const std::string& accountId);

// Member management
DRING_PUBLIC bool addConversationMember(const std::string& accountId,
                                        const std::string& conversationId,
                                        const std::string& contactUri);
DRING_PUBLIC bool removeConversationMember(const std::string& accountId,
                                           const std::string& conversationId,
                                           const std::string& contactUri);
DRING_PUBLIC std::vector<std::map<std::string, std::string>> getConversationMembers(
    const std::string& accountId, const std::string& conversationId);

// Message send/load
DRING_PUBLIC void sendMessage(const std::string& accountId,
                              const std::string& conversationId,
                              const std::string& message,
                              const std::string& parent);
DRING_PUBLIC void loadConversationMessages(const std::string& accountId,
                                           const std::string& conversationId,
                                           const std::string& fromMessage,
                                           size_t n);

struct DRING_PUBLIC ConversationSignal
{
    struct DRING_PUBLIC ConversationLoaded
    {
        constexpr static const char* name = "ConversationLoaded";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::vector<std::map<std::string, std::string>> /*messages*/);
    };
    struct DRING_PUBLIC MessageReceived
    {
        constexpr static const char* name = "MessageReceived";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::map<std::string, std::string> /*message*/);
    };
    struct DRING_PUBLIC ConversationRequestReceived
    {
        constexpr static const char* name = "ConversationRequestReceived";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::map<std::string, std::string> /*metadatas*/);
    };
    struct DRING_PUBLIC ConversationReady
    {
        constexpr static const char* name = "ConversationReady";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */);
    };
};

} // namespace DRing

#endif // DRING_CONVERSATIONI_H
