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

#include "jami.h"

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

// Conversation's infos management
DRING_PUBLIC void updateConversationInfos(const std::string& accountId,
                                          const std::string& conversationId,
                                          const std::map<std::string, std::string>& infos);
DRING_PUBLIC std::map<std::string, std::string> conversationInfos(const std::string& accountId,
                                                                  const std::string& conversationId);
DRING_PUBLIC void setConversationPreferences(const std::string& accountId,
                                             const std::string& conversationId,
                                             const std::map<std::string, std::string>& prefs);
DRING_PUBLIC std::map<std::string, std::string> getConversationPreferences(
    const std::string& accountId, const std::string& conversationId);

// Member management
DRING_PUBLIC void addConversationMember(const std::string& accountId,
                                        const std::string& conversationId,
                                        const std::string& contactUri);
DRING_PUBLIC void removeConversationMember(const std::string& accountId,
                                           const std::string& conversationId,
                                           const std::string& contactUri);
DRING_PUBLIC std::vector<std::map<std::string, std::string>> getConversationMembers(
    const std::string& accountId, const std::string& conversationId);

// Message send/load
DRING_PUBLIC void sendMessage(const std::string& accountId,
                              const std::string& conversationId,
                              const std::string& message,
                              const std::string& replyTo);
DRING_PUBLIC uint32_t loadConversationMessages(const std::string& accountId,
                                               const std::string& conversationId,
                                               const std::string& fromMessage,
                                               size_t n);
DRING_PUBLIC uint32_t loadConversationUntil(const std::string& accountId,
                                            const std::string& conversationId,
                                            const std::string& fromMessage,
                                            const std::string& toMessage);
DRING_PUBLIC uint32_t countInteractions(const std::string& accountId,
                                        const std::string& conversationId,
                                        const std::string& toId,
                                        const std::string& fromId,
                                        const std::string& authorUri);

struct DRING_PUBLIC ConversationSignal
{
    struct DRING_PUBLIC ConversationLoaded
    {
        constexpr static const char* name = "ConversationLoaded";
        using cb_type = void(uint32_t /* id */,
                             const std::string& /*accountId*/,
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
    struct DRING_PUBLIC ConversationProfileUpdated
    {
        constexpr static const char* name = "ConversationProfileUpdated";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::map<std::string, std::string> /*profile*/);
    };
    struct DRING_PUBLIC ConversationRequestReceived
    {
        constexpr static const char* name = "ConversationRequestReceived";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::map<std::string, std::string> /*metadatas*/);
    };
    struct DRING_PUBLIC ConversationRequestDeclined
    {
        constexpr static const char* name = "ConversationRequestDeclined";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */);
    };
    struct DRING_PUBLIC ConversationReady
    {
        constexpr static const char* name = "ConversationReady";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */);
    };
    struct DRING_PUBLIC ConversationRemoved
    {
        constexpr static const char* name = "ConversationRemoved";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */);
    };
    struct DRING_PUBLIC ConversationMemberEvent
    {
        constexpr static const char* name = "ConversationMemberEvent";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             const std::string& /* memberUri */,
                             int /* event 0 = add, 1 = joins, 2 = leave, 3 = banned */);
    };

    struct DRING_PUBLIC ConversationSyncFinished
    {
        constexpr static const char* name = "ConversationSyncFinished";
        using cb_type = void(const std::string& /*accountId*/);
    };

    struct DRING_PUBLIC CallConnectionRequest
    {
        constexpr static const char* name = "CallConnectionRequest";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*peerId*/,
                             bool hasVideo);
    };

    struct DRING_PUBLIC OnConversationError
    {
        constexpr static const char* name = "OnConversationError";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             int code,
                             const std::string& what);
    };

    // Preferences
    struct DRING_PUBLIC ConversationPreferencesUpdated
    {
        constexpr static const char* name = "ConversationPreferencesUpdated";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*conversationId*/,
                             std::map<std::string, std::string> /*preferences*/);
    };
};

} // namespace DRing

#endif // DRING_CONVERSATIONI_H
