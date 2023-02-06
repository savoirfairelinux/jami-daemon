/*
 *  Copyright (C) 2013-2023 Savoir-faire Linux Inc.
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

#ifndef LIBJAMI_CONVERSATIONI_H
#define LIBJAMI_CONVERSATIONI_H

#include "def.h"

#include <vector>
#include <map>
#include <string>

#include "jami.h"

namespace libjami {

// Conversation management
LIBJAMI_PUBLIC std::string startConversation(const std::string& accountId);
LIBJAMI_PUBLIC void acceptConversationRequest(const std::string& accountId,
                                              const std::string& conversationId);
LIBJAMI_PUBLIC void declineConversationRequest(const std::string& accountId,
                                               const std::string& conversationId);
LIBJAMI_PUBLIC bool removeConversation(const std::string& accountId,
                                       const std::string& conversationId);
LIBJAMI_PUBLIC std::vector<std::string> getConversations(const std::string& accountId);
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getConversationRequests(
    const std::string& accountId);

// Calls
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getActiveCalls(
    const std::string& accountId, const std::string& conversationId);

// Conversation's infos management
LIBJAMI_PUBLIC void updateConversationInfos(const std::string& accountId,
                                            const std::string& conversationId,
                                            const std::map<std::string, std::string>& infos);
LIBJAMI_PUBLIC std::map<std::string, std::string> conversationInfos(
    const std::string& accountId, const std::string& conversationId);
LIBJAMI_PUBLIC void setConversationPreferences(const std::string& accountId,
                                               const std::string& conversationId,
                                               const std::map<std::string, std::string>& prefs);
LIBJAMI_PUBLIC std::map<std::string, std::string> getConversationPreferences(
    const std::string& accountId, const std::string& conversationId);

// Member management
LIBJAMI_PUBLIC void addConversationMember(const std::string& accountId,
                                          const std::string& conversationId,
                                          const std::string& contactUri);
LIBJAMI_PUBLIC void removeConversationMember(const std::string& accountId,
                                             const std::string& conversationId,
                                             const std::string& contactUri);
LIBJAMI_PUBLIC std::vector<std::map<std::string, std::string>> getConversationMembers(
    const std::string& accountId, const std::string& conversationId);

// Message send/load
LIBJAMI_PUBLIC void sendMessage(const std::string& accountId,
                                const std::string& conversationId,
                                const std::string& message,
                                const std::string& replyTo,
                                const int32_t& flag = 0);
LIBJAMI_PUBLIC uint32_t loadConversationMessages(const std::string& accountId,
                                                 const std::string& conversationId,
                                                 const std::string& fromMessage,
                                                 size_t n);
LIBJAMI_PUBLIC uint32_t loadConversationUntil(const std::string& accountId,
                                              const std::string& conversationId,
                                              const std::string& fromMessage,
                                              const std::string& toMessage);
LIBJAMI_PUBLIC uint32_t countInteractions(const std::string& accountId,
                                          const std::string& conversationId,
                                          const std::string& toId,
                                          const std::string& fromId,
                                          const std::string& authorUri);
LIBJAMI_PUBLIC uint32_t searchConversation(const std::string& accountId,
                                           const std::string& conversationId,
                                           const std::string& author,
                                           const std::string& lastId,
                                           const std::string& regexSearch,
                                           const std::string& type,
                                           const int64_t& after,
                                           const int64_t& before,
                                           const uint32_t& maxResult,
                                           const int32_t& flag);

struct LIBJAMI_PUBLIC ConversationSignal
{
    struct LIBJAMI_PUBLIC ConversationLoaded
    {
        constexpr static const char* name = "ConversationLoaded";
        using cb_type = void(uint32_t /* id */,
                             const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::vector<std::map<std::string, std::string>> /*messages*/);
    };
    struct LIBJAMI_PUBLIC MessagesFound
    {
        constexpr static const char* name = "MessagesFound";
        using cb_type = void(uint32_t /* id */,
                             const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::vector<std::map<std::string, std::string>> /*messages*/);
    };
    struct LIBJAMI_PUBLIC MessageReceived
    {
        constexpr static const char* name = "MessageReceived";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::map<std::string, std::string> /*message*/);
    };
    struct LIBJAMI_PUBLIC ConversationProfileUpdated
    {
        constexpr static const char* name = "ConversationProfileUpdated";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::map<std::string, std::string> /*profile*/);
    };
    struct LIBJAMI_PUBLIC ConversationRequestReceived
    {
        constexpr static const char* name = "ConversationRequestReceived";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             std::map<std::string, std::string> /*metadatas*/);
    };
    struct LIBJAMI_PUBLIC ConversationRequestDeclined
    {
        constexpr static const char* name = "ConversationRequestDeclined";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */);
    };
    struct LIBJAMI_PUBLIC ConversationReady
    {
        constexpr static const char* name = "ConversationReady";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */);
    };
    struct LIBJAMI_PUBLIC ConversationRemoved
    {
        constexpr static const char* name = "ConversationRemoved";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */);
    };
    struct LIBJAMI_PUBLIC ConversationMemberEvent
    {
        constexpr static const char* name = "ConversationMemberEvent";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             const std::string& /* memberUri */,
                             int /* event 0 = add, 1 = joins, 2 = leave, 3 = banned */);
    };

    struct LIBJAMI_PUBLIC ConversationSyncFinished
    {
        constexpr static const char* name = "ConversationSyncFinished";
        using cb_type = void(const std::string& /*accountId*/);
    };

    struct LIBJAMI_PUBLIC CallConnectionRequest
    {
        constexpr static const char* name = "CallConnectionRequest";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*peerId*/,
                             bool hasVideo);
    };

    struct LIBJAMI_PUBLIC OnConversationError
    {
        constexpr static const char* name = "OnConversationError";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /* conversationId */,
                             int code,
                             const std::string& what);
    };

    // Preferences
    struct LIBJAMI_PUBLIC ConversationPreferencesUpdated
    {
        constexpr static const char* name = "ConversationPreferencesUpdated";
        using cb_type = void(const std::string& /*accountId*/,
                             const std::string& /*conversationId*/,
                             std::map<std::string, std::string> /*preferences*/);
    };
};

} // namespace libjami

#endif // LIBJAMI_CONVERSATIONI_H
