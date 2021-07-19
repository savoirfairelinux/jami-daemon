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

ConversationModule::ConversationModule(std::weak_ptr<JamiAccount>&& account,
                                       std::function<void()>&& needsSyncingCb)
    : account_(account)
    , needsSyncingCb_(needsSyncingCb)
{
    if (auto shared = account_.lock()) {
        accountId_ = shared->getAccountID();
    }
}

std::vector<std::string>
ConversationModule::getConversations() const
{
    std::vector<std::string> result;
    std::lock_guard<std::mutex> lk(convInfosMtx_);
    result.reserve(convInfos_.size());
    for (const auto& [key, conv] : convInfos_) {
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

void
ConversationModule::declineConversationRequest(const std::string& conversationId)
{
    std::lock_guard<std::mutex> lk(conversationsRequestsMtx_);
    auto it = conversationsRequests_.find(conversationId);
    if (it != conversationsRequests_.end()) {
        it->second.declined = std::time(nullptr);
        saveConvRequests();
    }

    if (needsSyncingCb_)
        needsSyncingCb_();
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
ConversationModule::setConvInfos(const std::map<std::string, ConvInfo>& newConv)
{
    convInfos_ = newConv;
}

void
ConversationModule::setConversationMembers(const std::string& convId,
                                           const std::vector<std::string>& members)
{
    auto convIt = convInfos_.find(convId);
    if (convIt != convInfos_.end()) {
        convIt->second.members = members;
        saveConvInfos();
    }
}

void
ConversationModule::saveConvInfos() const
{
    saveConvInfos(accountId_, convInfos_);
}

void
ConversationModule::saveConvInfos(const std::string& accountId,
                                  const std::map<std::string, ConvInfo>& conversations)
{
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId
                + DIR_SEPARATOR_STR "convInfo";
    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    msgpack::pack(file, conversations);
}

std::map<std::string, ConvInfo>
ConversationModule::convInfos(const std::string& accountId)
{
    std::map<std::string, ConvInfo> convInfos;
    try {
        // read file
        auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId;
        auto file = fileutils::loadFile("convInfo", path);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
        oh.get().convert(convInfos);
    } catch (const std::exception& e) {
        JAMI_WARN("[convInfo] error loading convInfo: %s", e.what());
    }
    return convInfos;
}

std::map<std::string, ConversationRequest>
ConversationModule::convRequests(const std::string& accountId)
{
    std::map<std::string, ConversationRequest> convRequests;
    try {
        // read file
        auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId;
        auto file = fileutils::loadFile("convRequests", path);
        // load values
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
        oh.get().convert(convRequests);
    } catch (const std::exception& e) {
        JAMI_WARN("[convInfo] error loading convInfo: %s", e.what());
    }
    return convRequests;
}

void
ConversationModule::addConvInfo(const ConvInfo& info)
{
    convInfos_[info.id] = info;
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
    saveConvRequests(accountId_, conversationsRequests_);
}

void
ConversationModule::saveConvRequests(
    const std::string& accountId,
    const std::map<std::string, ConversationRequest>& conversationsRequests)
{
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId
                + DIR_SEPARATOR_STR "convRequests";
    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    msgpack::pack(file, conversationsRequests);
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