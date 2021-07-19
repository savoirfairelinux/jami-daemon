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

#include "src/jamidht/jamiaccount.h"
#include "fileutils.h"

namespace jami {

ConversationModule::ConversationModule(std::weak_ptr<JamiAccount>&& account)
    : account_(account)
{
    if (auto shared = account_.lock()) {
        convInfoPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + shared->getAccountID()
                        + DIR_SEPARATOR_STR "convInfo";
        convReqPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR + shared->getAccountID()
                       + DIR_SEPARATOR_STR "convRequests";
        accountId_ = shared->getAccountID();
    }
}

std::vector<std::string>
ConversationModule::getConversations() const
{
    std::vector<std::string> result;
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    result.reserve(conversations_.size());
    for (const auto& [key, conv] : conversations_) {
        if (conv.removed)
            continue;
        result.emplace_back(key);
    }
    return result;
}

std::vector<std::map<std::string, std::string>>
ConversationModule::getConversationRequests() const
{
    std::vector<std::map<std::string, std::string>> requests;
    std::lock_guard<std::mutex> lk(conversationsRequestsMtx_);
    requests.reserve(conversationsRequests_.size());
    for (const auto& [id, request] : conversationsRequests_) {
        if (request.declined)
            continue; // Do not add declined requests
        requests.emplace_back(request.toMap());
    }
    return requests;
}

std::string
ConversationModule::startConversation(ConversationMode mode, const std::string& otherMember)
{
    // Create the conversation object
    // TODO link to jamiaccount
    /*auto conversation = std::make_shared<Conversation>(weak(), mode, otherMember);
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
    return convId;*/
    return {};
}

void
ConversationModule::setConversations(const std::map<std::string, ConvInfo>& newConv)
{
    conversations_ = newConv;
}

void
ConversationModule::setConversationMembers(const std::string& convId,
                                           const std::vector<std::string>& members)
{
    auto convIt = conversations_.find(convId);
    if (convIt != conversations_.end()) {
        convIt->second.members = members;
        saveConvInfos();
    }
}

void
ConversationModule::saveConvInfos() const
{
    std::ofstream file(convInfoPath_, std::ios::trunc | std::ios::binary);
    msgpack::pack(file, conversations_);
}

void
ConversationModule::addConversation(const ConvInfo& info)
{
    conversations_[info.id] = info;
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
    std::ofstream file(convReqPath_, std::ios::trunc | std::ios::binary);
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