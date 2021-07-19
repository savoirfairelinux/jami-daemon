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

#include "client/ring_signal.h"
#include "jamidht/account_manager.h"
#include "jamidht/jamiaccount.h"
#include "fileutils.h"

namespace jami {

ConversationModule::ConversationModule(
    std::weak_ptr<JamiAccount>&& account,
    std::function<void()>&& needsSyncingCb,
    std::function<void(std::string&&, std::map<std::string, std::string>&&)>&& sendMsgCb)
    : account_(account)
    , needsSyncingCb_(needsSyncingCb)
    , sendMsgCb_(sendMsgCb)
{
    if (auto shared = account_.lock()) {
        accountId_ = shared->getAccountID();
        deviceId_ = shared->currentDeviceId();
        username_ = shared->getUsername();
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
    needsSyncingCb_();
}

bool
ConversationModule::removeConversation(const std::string& conversationId)
{
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto it = conversations_.find(conversationId);
    if (it == conversations_.end()) {
        JAMI_ERR("Conversation %s doesn't exist", conversationId.c_str());
        return false;
    }
    auto members = it->second->getMembers();
    auto hasMembers = !(members.size() == 1
                        && username_.find(members[0]["uri"]) != std::string::npos);
    // Update convInfos
    std::unique_lock<std::mutex> lockCi(convInfosMtx_);
    auto itConv = convInfos_.find(conversationId);
    if (itConv != convInfos_.end()) {
        itConv->second.removed = std::time(nullptr);
        // Sync now, because it can take some time to really removes the datas
        if (hasMembers)
            needsSyncingCb_();
    }
    saveConvInfos();
    lockCi.unlock();
    auto commitId = it->second->leave();
    emitSignal<DRing::ConversationSignal::ConversationRemoved>(accountId_, conversationId);
    if (hasMembers) {
        JAMI_DBG() << "Wait that someone sync that user left conversation " << conversationId;
        // Commit that we left
        if (!commitId.empty()) {
            // Do not sync as it's synched by convInfos
            sendMessageNotification(*it->second, commitId, false);
        } else {
            JAMI_ERR("Failed to send message to conversation %s", conversationId.c_str());
        }
        // In this case, we wait that another peer sync the conversation
        // to definitely remove it from the device. This is to inform the
        // peer that we left the conversation and never want to receives
        // any messages
        return true;
    }
    lk.unlock();
    // Else we are the last member, so we can remove
    removeRepository(conversationId, true);
    return true;
}

void
ConversationModule::removeRepository(const std::string& conversationId, bool sync, bool force)
{
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto it = conversations_.find(conversationId);
    if (it != conversations_.end() && it->second && (force || it->second->isRemoving())) {
        try {
            if (it->second->mode() == ConversationMode::ONE_TO_ONE) {
                auto account = account_.lock();
                for (const auto& member : it->second->getInitialMembers()) {
                    if (member != account->getUsername()) {
                        account->accountManager()->removeContactConversation(
                            member); // TODO better way?
                    }
                }
            }
        } catch (const std::exception& e) {
            JAMI_ERR() << e.what();
        }
        JAMI_DBG() << "Remove conversation: " << conversationId;
        it->second->erase();
        conversations_.erase(it);
        lk.unlock();

        if (!sync)
            return;
        std::lock_guard<std::mutex> lkCi(convInfosMtx_);
        auto convIt = convInfos_.find(conversationId);
        if (convIt != convInfos_.end()) {
            convIt->second.erased = std::time(nullptr);
            needsSyncingCb_();
        }
        saveConvInfos();
    }
}

std::string
ConversationModule::startConversation(ConversationMode mode, const std::string& otherMember)
{
    // Create the conversation object
    auto conversation = std::make_shared<Conversation>(account_, mode, otherMember);
    auto convId = conversation->id();
    {
        std::lock_guard<std::mutex> lk(conversationsMtx_);
        conversations_[convId] = std::move(conversation);
    }

    // Update convInfo
    ConvInfo info;
    info.id = convId;
    info.created = std::time(nullptr);
    info.members.emplace_back(username_);
    if (!otherMember.empty())
        info.members.emplace_back(otherMember);
    addConvInfo(info);

    needsSyncingCb_();

    emitSignal<DRing::ConversationSignal::ConversationReady>(accountId_, convId);
    return convId;
}

uint32_t
ConversationModule::loadConversationMessages(const std::string& conversationId,
                                             const std::string& fromMessage,
                                             size_t n)
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(conversationId);
    if (conversation != conversations_.end() && conversation->second) {
        const uint32_t id = std::uniform_int_distribution<uint32_t>()(rand);
        conversation->second->loadMessages(
            [accountId = accountId_, conversationId, id](auto&& messages) {
                emitSignal<DRing::ConversationSignal::ConversationLoaded>(id,
                                                                          accountId,
                                                                          conversationId,
                                                                          messages);
            },
            fromMessage,
            n);
        return id;
    }
    return 0;
}

void
ConversationModule::sendMessageNotification(const Conversation& conversation,
                                            const std::string& commitId,
                                            bool sync)
{
    Json::Value message;
    message["id"] = conversation.id();
    message["commit"] = commitId;
    // TODO avoid lookup
    message["deviceId"] = deviceId_;
    Json::StreamWriterBuilder builder;
    const auto text = Json::writeString(builder, message);
    for (const auto& members : conversation.getMembers()) {
        auto uri = members.at("uri");
        // Do not send to ourself, it's synced via convInfos
        if (!sync && username_.find(uri) != std::string::npos)
            continue;
        // Announce to all members that a new message is sent
        sendMsgCb_(std::move(uri),
                   std::move(std::map<std::string, std::string> {
                       {"application/im-gitmessage-id", text}}));
    }
}
void
ConversationModule::sendMessageNotification(const std::string& conversationId,
                                            const std::string& commitId,
                                            bool sync)
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    auto it = conversations_.find(conversationId);
    if (it != conversations_.end() && it->second) {
        sendMessageNotification(*it->second, commitId, sync);
    }
}

void
ConversationModule::updateConversationInfos(const std::string& conversationId,
                                            const std::map<std::string, std::string>& infos,
                                            bool sync)
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    // Add a new member in the conversation
    auto it = conversations_.find(conversationId);
    if (it == conversations_.end()) {
        JAMI_ERR("Conversation %s doesn't exist", conversationId.c_str());
        return;
    }

    it->second->updateInfos(infos,
                            [this, conversationId, sync](bool ok, const std::string& commitId) {
                                if (ok && sync) {
                                    sendMessageNotification(conversationId, commitId, true);
                                } else if (sync)
                                    JAMI_WARN("Couldn't update infos on %s", conversationId.c_str());
                            });
}

std::map<std::string, std::string>
ConversationModule::conversationInfos(const std::string& conversationId) const
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    // Add a new member in the conversation
    auto it = conversations_.find(conversationId);
    if (it == conversations_.end() or not it->second) {
        std::lock_guard<std::mutex> lkCi(convInfosMtx_);
        auto itConv = convInfos_.find(conversationId);
        if (itConv == convInfos_.end()) {
            JAMI_ERR("Conversation %s doesn't exist", conversationId.c_str());
            return {};
        }
        return {{"syncing", "true"}};
    }

    return it->second->infos();
}

std::vector<uint8_t>
ConversationModule::conversationVCard(const std::string& conversationId) const
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    // Add a new member in the conversation
    auto it = conversations_.find(conversationId);
    if (it == conversations_.end() || !it->second) {
        JAMI_ERR("Conversation %s doesn't exist", conversationId.c_str());
        return {};
    }

    return it->second->vCard();
}

void
ConversationModule::addConversationMember(const std::string& conversationId,
                                   const std::string& contactUri,
                                   bool sendRequest)
{
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    // Add a new member in the conversation
    auto it = conversations_.find(conversationId);
    if (it == conversations_.end()) {
        JAMI_ERR("Conversation %s doesn't exist", conversationId.c_str());
        return;
    }

    if (it->second->isMember(contactUri, true)) {
        JAMI_DBG("%s is already a member of %s, resend invite",
                 contactUri.c_str(),
                 conversationId.c_str());
        // Note: This should not be necessary, but if for whatever reason the other side didn't join
        // we should not forbid new invites
        auto invite = it->second->generateInvitation();
        lk.unlock();
        sendMsgCb_(contactUri, invite);
        return;
    }

    it->second->addMember(contactUri,
                          [this,
                           conversationId,
                           sendRequest,
                           contactUri](bool ok, const std::string& commitId) {
                              if (ok) {
                                  auto shared = w.lock();
                                  if (shared) {
                                      std::unique_lock<std::mutex> lk(shared->conversationsMtx_);
                                      auto it = shared->conversations_.find(conversationId);
                                      if (it != shared->conversations_.end() && it->second) {
                                          sendMessageNotification(*it->second, commitId, true); // For the other members
                                          if (sendRequest) {
                                              auto invite = it->second->generateInvitation();
                                              lk.unlock();
                                              sendMsgCb_(contactUri, invite);
                                          }
                                      }
                                  }
                              }
                          });
}

void
ConversationModule::removeConversationMember(const std::string& conversationId,
                                             const std::string& contactUri,
                                             bool isDevice)
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    auto it = conversations_.find(conversationId);
    if (it != conversations_.end() && it->second) {
        it->second->removeMember(contactUri,
                                 isDevice,
                                 [this, conversationId](bool ok, const std::string& commitId) {
                                     if (ok) {
                                         sendMessageNotification(conversationId, commitId, true);
                                     }
                                 });
    }
}

std::vector<std::map<std::string, std::string>>
ConversationModule::getConversationMembers(const std::string& conversationId) const
{
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(conversationId);
    if (conversation != conversations_.end() && conversation->second)
        return conversation->second->getMembers(true, true);

    lk.unlock();
    auto convIt = convInfos_.find(conversationId);
    if (convIt != convInfos_.end()) {
        std::vector<std::map<std::string, std::string>> result;
        result.reserve(convIt->second.members.size());
        for (const auto& uri : convIt->second.members) {
            result.emplace_back(std::map<std::string, std::string> {{"uri", uri}});
        }
        return result;
    }
    return {};
}

// Message send/load
void
ConversationModule::sendMessage(const std::string& conversationId,
                         const std::string& message,
                         const std::string& parent,
                         const std::string& type,
                         bool announce,
                         const OnDoneCb& cb)
{
    Json::Value json;
    json["body"] = message;
    json["type"] = type;
    sendMessage(conversationId, json, parent, announce, cb);
}

void
ConversationModule::sendMessage(const std::string& conversationId,
                         const Json::Value& value,
                         const std::string& parent,
                         bool announce,
                         const OnDoneCb& cb)
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(conversationId);
    if (conversation != conversations_.end() && conversation->second) {
        conversation->second
            ->sendMessage(value,
                          parent,
                          [this,
                           conversationId,
                           announce,
                           cb = std::move(cb)](bool ok, const std::string& commitId) {
                              if (cb)
                                  cb(ok, commitId);
                              if (!announce)
                                  return;
                              if (ok)
                                  sendMessageNotification(conversationId, commitId, true);
                              else
                                  JAMI_ERR("Failed to send message to conversation %s",
                                           conversationId.c_str());
                          });
    }
}


uint32_t
ConversationModule::countInteractions(const std::string& convId,
                               const std::string& toId,
                               const std::string& fromId) const
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(convId);
    if (conversation != conversations_.end() && conversation->second) {
        return conversation->second->countInteractions(toId, fromId);
    }
    return 0;
}

void
ConversationModule::setConvInfos(const std::map<std::string, ConvInfo>& newConv)
{
    convInfos_ = newConv;
    saveConvInfos();
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

void
ConversationModule::onNeedConversationRequest(const std::string& from, const std::string& conversationId)
{
    // Check if conversation exists
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto itConv = conversations_.find(conversationId);
    if (itConv != conversations_.end() && !itConv->second->isRemoving()) {
        // Check if isMember
        if (!itConv->second->isMember(from, true)) {
            JAMI_WARN("%s is asking a new invite for %s, but not a member",
                      from.c_str(),
                      conversationId.c_str());
            return;
        }

        // Send new invite
        auto invite = itConv->second->generateInvitation();
        lk.unlock();
        JAMI_DBG("%s is asking a new invite for %s", from.c_str(), conversationId.c_str());
        sendMsgCb_(from, invite);
    }
}

void
ConversationModule::onConversationRequest(const std::string& from, const Json::Value& value)
{
    ConversationRequest req(value);
    JAMI_INFO("[Account %s] Receive a new conversation request for conversation %s from %s",
              accountId_.c_str(),
              req.conversationId.c_str(),
              from.c_str());
    auto convId = req.conversationId;
    req.from = from;

    if (getRequest(convId) != std::nullopt) {
        JAMI_INFO("[Account %s] Received a request for a conversation already existing. "
                  "Ignore",
                  accountId_.c_str());
        return;
    }
    req.received = std::time(nullptr);
    auto reqMap = req.toMap();
    addConversationRequest(convId, std::move(req));
    // Note: no need to sync here because over connected devices should receives
    // the same conversation request. Will sync when the conversation will be added

    emitSignal<DRing::ConversationSignal::ConversationRequestReceived>(accountId_, convId, reqMap);
}

void
ConversationModule::checkIfRemoveForCompat(const std::string& peerUri)
{
    auto convId = getOneToOneConversation(peerUri);
    if (convId.empty())
        return;
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto it = conversations_.find(convId);
    if (it == conversations_.end()) {
        JAMI_ERR("Conversation %s doesn't exist", convId.c_str());
        return;
    }
    // We will only removes the conversation if the member is invited
    // the contact can have mutiple devices with only some with swarm
    // support, in this case, just go with recent versions.
    if (it->second->isMember(peerUri))
        return;
    lk.unlock();
    removeConversation(convId);
}

void
ConversationModule::addCallHistoryMessage(const std::string& uri, uint64_t duration_ms)
{
    auto finalUri = uri.substr(0, uri.find("@ring.dht"));
    finalUri = finalUri.substr(0, uri.find("@jami.dht"));
    auto convId = getOneToOneConversation(finalUri);
    if (!convId.empty()) {
        Json::Value value;
        value["to"] = finalUri;
        value["type"] = "application/call-history+json";
        value["duration"] = std::to_string(duration_ms);
        sendMessage(convId, value);
    }
}

void
ConversationModule::checkConversationsEvents()
{
    bool hasHandler = conversationsEventHandler and not conversationsEventHandler->isCancelled();
    std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
    if (not pendingConversationsFetch_.empty() and not hasHandler) {
        conversationsEventHandler = Manager::instance().scheduler().scheduleAtFixedRate(
            [w = weak()] {
                if (auto this_ = w.lock())
                    return this_->handlePendingConversations();
                return false;
            },
            std::chrono::milliseconds(10));
    } else if (pendingConversationsFetch_.empty() and hasHandler) {
        conversationsEventHandler->cancel();
        conversationsEventHandler.reset();
    }
}

std::string
ConversationModule::getOneToOneConversation(const std::string& uri) const
{
    // Note it's important to check getUsername(), else
    // removing self can remove all conversations
    auto isSelf = uri == username_;
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    for (const auto& [key, conv] : conversations_) {
        if (!conv)
            continue;
        try {
            if (conv->mode() == ConversationMode::ONE_TO_ONE) {
                auto initMembers = conv->getInitialMembers();
                if (isSelf && initMembers.size() == 1)
                    return key;
                if (std::find(initMembers.begin(), initMembers.end(), uri) != initMembers.end())
                    return key;
            }
        } catch (const std::exception& e) {
            JAMI_ERR() << e.what();
        }
    }
    return {};
}

bool
ConversationModule::handlePendingConversations()
{
    std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
    for (auto it = pendingConversationsFetch_.begin(); it != pendingConversationsFetch_.end();) {
        if (it->second.ready) {
            dht::ThreadPool::io().run([this, // TODO weak()? Verify lifetime
                                       conversationId = it->first,
                                       deviceId = it->second.deviceId]() {
                // Clone and store conversation
                try {
                    auto conversation = std::make_shared<Conversation>(account_, deviceId, conversationId);
                    if (!conversation->isMember(username_, true)) {
                        JAMI_ERR("Conversation cloned but doesn't seems to be a valid member");
                        conversation->erase();
                        return;
                    }
                    if (conversation) {
                        auto commitId = conversation->join();
                        {
                            std::lock_guard<std::mutex> lk(conversationsMtx_);
                            conversations_.emplace(conversationId, std::move(conversation));
                        }
                        if (!commitId.empty())
                            sendMessageNotification(conversationId, commitId, false);
                        // Inform user that the conversation is ready
                        emitSignal<DRing::ConversationSignal::ConversationReady>(accountId_,
                                                                                 conversationId);
                    }
                } catch (const std::exception& e) {
                    emitSignal<DRing::ConversationSignal::OnConversationError>(accountId_,
                                                                               conversationId,
                                                                               EFETCH,
                                                                               e.what());
                    JAMI_WARN("Something went wrong when cloning conversation: %s", e.what());
                }
            });
            it = pendingConversationsFetch_.erase(it);
        } else {
            ++it;
        }
    }
    return !pendingConversationsFetch_.empty();
}

} // namespace jami