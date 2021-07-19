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

#include "conversation_module.h"

#include <fstream>

namespace jami {

ConversationModule::ConversationModule() {}

std::string
ConversationModule::startConversation(ConversationMode mode, const std::string& otherMember)
{
    // Create the conversation object
    // TODO link to jamiaccount
    auto conversation = std::make_shared<Conversation>(weak(), mode, otherMember);
    auto convId = conversation->id();
    {
        std::lock_guard<std::mutex> lk(conversationsMtx_);
        conversations_[convId] = std::move(conversation);
    }

    // Update convInfo
    ConvInfo info;
    info.id = convId;
    info.created = std::time(nullptr);
    info.members.emplace_back(getUsername());
    if (!otherMember.empty())
        info.members.emplace_back(otherMember);
    addConversation(info);
    saveConvInfos();

    if (needsSyncingCb_)
        needsSyncingCb_();

    emitSignal<DRing::ConversationSignal::ConversationReady>(accountID_, convId);
    return convId;
}

void
ConversationModule::setConversations(const std::vector<ConvInfo>& newConv)
{
    conversations_ = newConv;
}

void
ConversationModule::setConversationMembers(const std::string& convId,
                                           const std::vector<std::string>& members)
{
    for (auto& ci : conversations_) {
        if (ci.id == convId) {
            ci.members = members;
            saveConvInfos();
        }
    }
}

void
ConversationModule::saveConvInfos() const
{
    // TODO PATH
    std::ofstream file(info_->contacts->path() + DIR_SEPARATOR_STR "convInfo",
                       std::ios::trunc | std::ios::binary);
    msgpack::pack(file, info_->conversations_);
}

void
ConversationModule::addConversation(const ConvInfo& info)
{
    conversations_.emplace_back(info);
    saveConvInfos();
}

void
ConversationModule::setConversationsRequests(
    const std::map<std::string, ConversationRequest>& newConvReq)
{
    std::lock_guard<std::mutex> lk(conversationsRequestsMtx_);
    conversationsRequests_ = newConvReq;
    saveConvRequests();
}

void
ConversationModule::saveConvRequests() const
{
    // TODO PATH
    std::ofstream file(info_->contacts->path() + DIR_SEPARATOR_STR "convRequests",
                       std::ios::trunc | std::ios::binary);
    msgpack::pack(file, conversationsRequests_);
}

std::optional<ConversationRequest>
ConversationModule::getRequest(const std::string& id) const
{
    std::lock_guard<std::mutex> lk(conversationsRequestsMtx_);
    auto it = conversationsRequests_.find(id);
    if (it != conversationsRequests_.end())
        return it->second;
    return std::nullopt;
}

void
ConversationModule::addConversationRequest(const std::string& id, const ConversationRequest& req)
{
    std::lock_guard<std::mutex> lk(conversationsRequestsMtx_);
    conversationsRequests_[id] = req;
    saveConvRequests();
}

void
ConversationModule::rmConversationRequest(const std::string& id)
{
    std::lock_guard<std::mutex> lk(conversationsRequestsMtx_);
    conversationsRequests_.erase(id);
    saveConvRequests();
}

} // namespace jami