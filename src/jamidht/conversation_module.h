/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#pragma once

#include "src/jamidht/conversation.h"
#include "src/jamidht/conversationrepository.h"

#include <mutex>

namespace jami {

class ConversationModule // TODO Module interface
{
public:
    ConversationModule(std::weak_ptr<JamiAccount>&& account_,
                       std::function<void()>&& needsSyncingCb);
    ~ConversationModule() = default;

    std::vector<std::string> getConversations() const;
    std::vector<std::map<std::string, std::string>> getConversationRequests() const;

    void declineConversationRequest(const std::string& conversationId);

    // TODO load
    // TODO needsSyncing, sendDhtMessage, sendMessageNotification

    // TODO remove accountManager::conversations/conversationsRequests

    std::string startConversation(ConversationMode mode = ConversationMode::INVITES_ONLY,
                                  const std::string& otherMember = "");

    // The following methods modify what is stored on the disk
    void saveConvInfos() const;
    static void saveConvInfos(const std::string& accountId,
                              const std::map<std::string, ConvInfo>& conversations);
    static std::map<std::string, ConvInfo> convInfos(const std::string& accountId);
    void saveConvRequests() const;
    static std::map<std::string, ConversationRequest> convRequests(const std::string& accountId);
    static void saveConvRequests(
        const std::string& accountId,
        const std::map<std::string, ConversationRequest>& conversationsRequests);
    void setConvInfos(const std::map<std::string, ConvInfo>& newConv);
    void addConvInfo(const ConvInfo& info);
    void setConversationMembers(const std::string& convId, const std::vector<std::string>& members);
    void setConversationsRequests(const std::map<std::string, ConversationRequest>& newConvReq);
    std::optional<ConversationRequest> getRequest(const std::string& id) const;
    void addConversationRequest(const std::string& id, const ConversationRequest& req);
    void rmConversationRequest(const std::string& id);

    bool isConversation(const std::string& convId) const
    {
        std::lock_guard<std::mutex> lk(conversationsMtx_);
        return conversations_.find(convId) != conversations_.end();
    }

private:
    // The following informations are stored on the disk
    mutable std::mutex convInfosMtx_;
    std::map<std::string, ConvInfo> convInfos_;
    mutable std::mutex conversationsRequestsMtx_;
    std::map<std::string, ConversationRequest> conversationsRequests_;

    std::function<void()> needsSyncingCb_;

    /** Conversations */
    mutable std::mutex conversationsMtx_ {};
    std::map<std::string, std::shared_ptr<Conversation>> conversations_;

    std::weak_ptr<JamiAccount> account_;
    std::string accountId_ {};
};

} // namespace jami