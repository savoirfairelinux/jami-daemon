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

namespace jami {

class ConversationModule // TODO Module interface
{
public:
    ConversationModule();
    ~ConversationModule() = default;

    // TODO needsSyncing, sendDhtMessage, sendMessageNotification

    // TODO remove accountManager::conversations/conversationsRequests

    std::string startConversation(ConversationMode mode = ConversationMode::INVITES_ONLY,
                                  const std::string& otherMember = "");

private:
    // The following methods modify what is stored on the disk
    void saveConvInfos() const;
    void saveConvRequests() const;
    void setConversations(const std::vector<ConvInfo>& newConv);
    void setConversationMembers(const std::string& convId, const std::vector<std::string>& members);
    void addConversation(const ConvInfo& info);
    void setConversationsRequests(const std::map<std::string, ConversationRequest>& newConvReq);
    std::optional<ConversationRequest> getRequest(const std::string& id) const;
    void addConversationRequest(const std::string& id, const ConversationRequest& req);
    void rmConversationRequest(const std::string& id);

    // The following informations are stored on the disk
    std::vector<ConvInfo> conversations_;
    mutable std::mutex conversationsRequestsMtx_;
    std::map<std::string, ConversationRequest> conversationsRequests_;
};

} // namespace jami