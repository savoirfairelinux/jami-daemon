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

#include <opendht/thread_pool.h>

#include "client/ring_signal.h"
#include "fileutils.h"
#include "jamidht/account_manager.h"
#include "jamidht/jamiaccount.h"
#include "manager.h"
#include "vcard.h"

namespace jami {

ConversationModule::ConversationModule(
    std::weak_ptr<JamiAccount>&& account,
    std::function<void()>&& needsSyncingCb,
    std::function<void(const std::string&, std::map<std::string, std::string>&&)>&& sendMsgCb,
    OnNeedGitCb&& onNeedGitSocket)
    : account_(account)
    , needsSyncingCb_(needsSyncingCb)
    , sendMsgCb_(sendMsgCb)
    , onNeedGitSocket_(onNeedGitSocket)
{
    if (auto shared = account_.lock()) {
        accountId_ = shared->getAccountID();
        deviceId_ = shared->currentDeviceId();
        username_ = shared->getUsername();
    }
    loadConversations();
}

void
ConversationModule::loadConversations()
{
    JAMI_INFO("[Account %s] Start loading conversations…", accountId_.c_str());
    auto conversationsRepositories = fileutils::readDirectory(
        fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId_ + DIR_SEPARATOR_STR
        + "conversations");
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    conversations_.clear();
    for (const auto& repository : conversationsRepositories) {
        try {
            auto conv = std::make_shared<Conversation>(account_, repository);
            conversations_.emplace(repository, std::move(conv));
        } catch (const std::logic_error& e) {
            JAMI_WARN("[Account %s] Conversations not loaded : %s", accountId_.c_str(), e.what());
        }
    }
    convInfos_ = convInfos(accountId_);
    conversationsRequests_ = convRequests(accountId_);

    for (auto& [key, info] : convInfos_) {
        auto itConv = conversations_.find(info.id);
        if (itConv != conversations_.end() && info.removed)
            itConv->second->setRemovingFlag();
    }

    JAMI_INFO("[Account %s] Conversations loaded!", accountId_.c_str());
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
ConversationModule::cloneConversation(const std::string& deviceId,
                                      const std::string&,
                                      const std::string& convId)
{
    JAMI_DBG("[Account %s] Clone conversation on device %s", accountId_.c_str(), deviceId.c_str());

    if (!isConversation(convId)) {
        {
            std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
            auto it = pendingConversationsFetch_.find(convId);
            // Note: here we don't return and connect to all members
            // the first that will successfully connect will be used for
            // cloning.
            // This avoid the case when we receives conversations from sync +
            // clone from infos. Both will be used
            if (it == pendingConversationsFetch_.end()) {
                pendingConversationsFetch_[convId] = PendingConversationFetch {};
            }
        }
        onNeedGitSocket_(convId, deviceId, [=](const auto& channel) {
            auto acc = account_.lock();
            std::unique_lock<std::mutex> lk(pendingConversationsFetchMtx_);
            auto& pending = pendingConversationsFetch_[convId];
            if (channel && !pending.ready) {
                pending.ready = true;
                pending.deviceId = channel->deviceId().toString();
                lk.unlock();
                // Save the git socket
                acc->addGitSocket(channel->deviceId(), convId, channel); // TODO move from there
                checkConversationsEvents();
                return true;
            }
            return false;
        });

        JAMI_INFO("[Account %s] New conversation detected: %s. Ask device %s to clone it",
                  accountId_.c_str(),
                  convId.c_str(),
                  deviceId.c_str());
    } else {
        JAMI_INFO("[Account %s] Already have conversation %s", accountId_.c_str(), convId.c_str());
    }
}

void
ConversationModule::fetchNewCommits(const std::string& peer,
                                    const std::string& deviceId,
                                    const std::string& conversationId,
                                    const std::string& commitId)
{
    JAMI_DBG("[Account %s] fetch commits for peer %s on device %s",
             accountId_.c_str(),
             peer.c_str(),
             deviceId.c_str());

    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(conversationId);
    if (conversation != conversations_.end() && conversation->second) {
        if (!conversation->second->isMember(peer, true)) {
            JAMI_WARN("[Account %s] %s is not a member of %s",
                      accountId_.c_str(),
                      peer.c_str(),
                      conversationId.c_str());
            return;
        }
        if (conversation->second->isBanned(deviceId)) {
            JAMI_WARN("[Account %s] %s is a banned device in conversation %s",
                      accountId_.c_str(),
                      deviceId.c_str(),
                      conversationId.c_str());
            return;
        }

        // Retrieve current last message
        auto lastMessageId = conversation->second->lastCommitId();
        if (lastMessageId.empty()) {
            JAMI_ERR("[Account %s] No message detected. This is a bug", accountId_.c_str());
            return;
        }

        {
            std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
            pendingConversationsFetch_[conversationId] = PendingConversationFetch {};
        }
        onNeedGitSocket_(conversationId,
                         deviceId,
                         [this,
                          conversationId = std::move(conversationId),
                          peer = std::move(peer),
                          deviceId = std::move(deviceId),
                          commitId = std::move(commitId)](const auto& channel) {
                             {
                                 std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
                                 pendingConversationsFetch_.erase(conversationId);
                             }
                             auto conversation = conversations_.find(conversationId);
                             auto acc = account_.lock();
                             if (!channel || !acc || conversation == conversations_.end()
                                 || !conversation->second)
                                 return false;
                             acc->addGitSocket(channel->deviceId(),
                                               conversationId,
                                               channel); // TODO move from there
                             conversation->second->sync(
                                 peer,
                                 deviceId,
                                 [this,
                                  conversationId = std::move(conversationId),
                                  peer = std::move(peer),
                                  deviceId = std::move(deviceId),
                                  commitId = std::move(commitId)](bool ok) {
                                     if (!ok) {
                                         JAMI_WARN("[Account %s] Could not fetch new commit from "
                                                   "%s for %s, other "
                                                   "peer may be disconnected",
                                                   accountId_.c_str(),
                                                   deviceId.c_str(),
                                                   conversationId.c_str());
                                         JAMI_INFO("[Account %s] Relaunch sync with %s for %s",
                                                   accountId_.c_str(),
                                                   deviceId.c_str(),
                                                   conversationId.c_str());
                                         // TODO shutdown + remove git socket?

                                         // runOnMainThread([this,
                                         // conversationId=std::move(conversationId),
                                         // peer=std::move(peer), deviceId=std::move(deviceId),
                                         // commitId=std::move(commitId)]() { //// TODO weak
                                         //    fetchNewCommits(peer, deviceId, conversationId,
                                         //    commitId);
                                         //});
                                     }
                                 },
                                 commitId);
                             return true;
                         });
    } else {
        if (getRequest(conversationId) != std::nullopt)
            return;
        {
            // Check if the conversation is cloning
            std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
            if (pendingConversationsFetch_.find(conversationId) != pendingConversationsFetch_.end())
                return;
        }
        if (conversation != conversations_.end()) {
            cloneConversation(deviceId, peer, conversationId);
            return;
        }
        lk.unlock();
        JAMI_WARN("[Account %s] Could not find conversation %s, ask for an invite",
                  accountId_.c_str(),
                  conversationId.c_str());
        sendMsgCb_(peer,
                   std::move(std::map<std::string, std::string> {
                       {"application/invite", conversationId}}));
    }
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

void
ConversationModule::setFetched(const std::string& conversationId, const std::string& deviceId)
{
    auto remove = false;
    {
        std::unique_lock<std::mutex> lk(conversationsMtx_);
        auto it = conversations_.find(conversationId);
        if (it != conversations_.end() && it->second) {
            remove = it->second->isRemoving();
            it->second->hasFetched(deviceId);
        }
    }
    if (remove)
        removeRepository(conversationId, true);
}

void
ConversationModule::onNewGitCommit(const std::string& peer,
                                   const std::string& deviceId,
                                   const std::string& conversationId,
                                   const std::string& commitId)
{
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto itConv = convInfos_.find(conversationId);
    if (itConv != convInfos_.end() && itConv->second.removed)
        return; // ignore new commits for removed conversation
    JAMI_DBG("[Account %s] on new commit notification from %s, for %s, commit %s",
             accountId_.c_str(),
             peer.c_str(),
             conversationId.c_str(),
             commitId.c_str());
    lk.unlock();
    fetchNewCommits(peer, deviceId, conversationId, commitId);
}

void
ConversationModule::removeContact(const std::string& uri, bool ban)
{
    // Remove related conversation
    auto isSelf = uri == username_;
    bool updateConvInfos = false; // TODO bypass
    std::vector<std::string> toRm;
    {
        std::lock_guard<std::mutex> lk(conversationsMtx_);
        for (auto& [convId, conv] : convInfos_) {
            auto itConv = conversations_.find(convId);
            if (itConv != conversations_.end() && itConv->second) {
                try {
                    // Note it's important to check getUsername(), else
                    // removing self can remove all conversations
                    if (itConv->second->mode() == ConversationMode::ONE_TO_ONE) {
                        auto initMembers = itConv->second->getInitialMembers();
                        if ((isSelf && initMembers.size() == 1)
                            || std::find(initMembers.begin(), initMembers.end(), uri)
                                   != initMembers.end())
                            toRm.emplace_back(convId);
                    }
                } catch (const std::exception& e) {
                    JAMI_WARN("%s", e.what());
                }
            } else if (std::find(conv.members.begin(), conv.members.end(), uri)
                       != conv.members.end()) {
                // It's syncing with uri, mark as removed!
                conv.removed = std::time(nullptr);
                updateConvInfos = true;
            }
        }
    }
    if (updateConvInfos)
        saveConvInfos();
    // Note, if we ban the device, we don't send the leave cause the other peer will just
    // never got the notifications, so just erase the datas
    for (const auto& id : toRm)
        if (ban)
            removeRepository(id, false, true);
        else
            removeConversation(id);
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

void
ConversationModule::onTrustRequest(const std::string& uri,
                                   const std::string& conversationId,
                                   const std::vector<uint8_t>& payload,
                                   time_t received)
{
    {
        std::lock_guard<std::mutex> lk(conversationsMtx_);
        auto itConv = conversations_.find(conversationId);
        if (itConv != conversations_.end()) {
            JAMI_INFO("[Account %s] Received a request for a conversation "
                      "already handled. Ignore",
                      accountId_.c_str());
            return;
        }
    }
    if (getRequest(conversationId) != std::nullopt) {
        JAMI_INFO("[Account %s] Received a request for a conversation "
                  "already existing. Ignore",
                  accountId_.c_str());
        return;
    }
    emitSignal<DRing::ConfigurationSignal::IncomingTrustRequest>(accountId_,
                                                                 conversationId,
                                                                 uri,
                                                                 payload,
                                                                 received);
    ConversationRequest req;
    req.from = uri;
    req.conversationId = conversationId;
    req.received = std::time(nullptr);
    auto details = vCard::utils::toMap(
        std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size()));
    req.metadatas = ConversationRepository::infosFromVCard(details);
    auto reqMap = req.toMap();
    addConversationRequest(conversationId, std::move(req));
    emitSignal<DRing::ConversationSignal::ConversationRequestReceived>(accountId_,
                                                                       conversationId,
                                                                       reqMap);
}

void
ConversationModule::syncConversations(const std::string& peer, const std::string& deviceId)
{
    // Sync conversations where peer is member
    std::set<std::string> toFetch;
    std::set<std::string> toClone;
    {
        std::lock_guard<std::mutex> lk(convInfosMtx_);
        for (const auto& [key, ci] : convInfos_) {
            auto it = conversations_.find(key);
            if (it != conversations_.end() && it->second) {
                if (!it->second->isRemoving() && it->second->isMember(peer, false))
                    toFetch.emplace(key);
            } else if (std::find(ci.members.begin(), ci.members.end(), peer) != ci.members.end()
                       && !ci.removed) {
                // In this case the conversation was never cloned (can be after an import)
                toClone.emplace(key);
            }
        }
    }
    for (const auto& cid : toFetch)
        fetchNewCommits(peer, deviceId, cid);
    for (const auto& cid : toClone)
        cloneConversation(deviceId, peer, cid);
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
    auto acc = account_.lock();
    auto conversation = conversations_.find(conversationId);
    if (acc && conversation != conversations_.end() && conversation->second) {
        const uint32_t id = std::uniform_int_distribution<uint32_t> {}(acc->rand);
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

bool
ConversationModule::onFileChannelRequest(const std::string& conversationId,
                                         const std::string& member,
                                         const std::string& fileId,
                                         bool verifyShaSum) const
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(conversationId);
    if (conversation != conversations_.end() && conversation->second)
        return conversation->second->onFileChannelRequest(member, fileId, verifyShaSum);
    return false;
}

std::shared_ptr<TransferManager>
ConversationModule::dataTransfer(const std::string& id) const
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(id);
    if (conversation != conversations_.end() && conversation->second)
        return conversation->second->dataTransfer();
    return {};
}

bool
ConversationModule::downloadFile(const std::string& conversationId,
                                 const std::string& interactionId,
                                 const std::string& fileId,
                                 const std::string& path,
                                 size_t start,
                                 size_t end)
{
    std::string sha3sum = {};
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(conversationId);
    if (conversation == conversations_.end() || !conversation->second)
        return false;

    return conversation->second->downloadFile(interactionId, fileId, path, "", "", start, end);
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
        sendMsgCb_(uri,
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
        sendMsgCb_(contactUri, std::move(invite));
        return;
    }

    it->second->addMember(contactUri,
                          [this,
                           conversationId,
                           sendRequest,
                           contactUri](bool ok, const std::string& commitId) {
                              if (ok) {
                                  std::unique_lock<std::mutex> lk(conversationsMtx_);
                                  auto it = conversations_.find(conversationId);
                                  if (it != conversations_.end() && it->second) {
                                      sendMessageNotification(*it->second,
                                                              commitId,
                                                              true); // For the other members
                                      if (sendRequest) {
                                          auto invite = it->second->generateInvitation();
                                          lk.unlock();
                                          sendMsgCb_(contactUri, std::move(invite));
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
        conversation->second->sendMessage(
            value,
            parent,
            [this, conversationId, announce, cb = std::move(cb)](bool ok,
                                                                 const std::string& commitId) {
                if (cb)
                    cb(ok, commitId);
                if (!announce)
                    return;
                if (ok)
                    sendMessageNotification(conversationId, commitId, true);
                else
                    JAMI_ERR("Failed to send message to conversation %s", conversationId.c_str());
            });
    }
}

void
ConversationModule::onMessageDisplayed(const std::string& peer,
                                       const std::string& conversationId,
                                       const std::string& interactionId)
{
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(conversationId);
    if (conversation != conversations_.end() && conversation->second) {
        conversation->second->setMessageDisplayed(peer, interactionId);
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
ConversationModule::onSyncData(const SyncMsg& msg,
                               const std::string& peerId,
                               const std::string& deviceId)
{
    for (const auto& [key, convInfo] : msg.c) {
        auto convId = convInfo.id;
        auto removed = convInfo.removed;
        rmConversationRequest(convId);
        if (not removed) {
            // If multi devices, it can detect a conversation that was already
            // removed, so just check if the convinfo contains a removed conv
            auto itConv = convInfos_.find(convId);
            if (itConv != convInfos_.end() && itConv->second.removed)
                continue;
            cloneConversation(deviceId, peerId, convId);
        } else {
            {
                std::lock_guard<std::mutex> lk(conversationsMtx_);
                auto itConv = conversations_.find(convId);
                if (itConv != conversations_.end() && !itConv->second->isRemoving()) {
                    emitSignal<DRing::ConversationSignal::ConversationRemoved>(accountId_, convId);
                    itConv->second->setRemovingFlag();
                }
            }
            auto ci = convInfos_; // TODO bypass setConvInfos & mutex
            auto itConv = ci.find(convId);
            if (itConv != ci.end()) {
                itConv->second.removed = std::time(nullptr);
                if (convInfo.erased) {
                    itConv->second.erased = std::time(nullptr);
                    removeRepository(convId, false);
                }
                break;
            }
            setConvInfos(std::move(ci));
        }
    }

    for (const auto& [convId, req] : msg.cr) {
        if (isConversation(convId)) {
            // Already accepted request
            rmConversationRequest(convId);
            continue;
        }

        // New request
        addConversationRequest(convId, req);

        if (req.declined != 0)
            continue; // Request removed, do not emit signal

        JAMI_INFO("[Account %s] New request detected for conversation %s (device %s)",
                  accountId_.c_str(),
                  convId.c_str(),
                  deviceId.c_str());

        emitSignal<DRing::ConversationSignal::ConversationRequestReceived>(accountId_,
                                                                           convId,
                                                                           req.toMap());
    }
}

void
ConversationModule::setConvInfos(const std::map<std::string, ConvInfo>& newConv)
{
    convInfos_ = newConv;
    saveConvInfos(); // Still useful?
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
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId + DIR_SEPARATOR_STR
                + "convInfo";
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
ConversationModule::acceptConversationRequest(const std::string& conversationId)
{
    auto acc = account_.lock();
    // For all conversation members, try to open a git channel with this conversation ID
    auto request = getRequest(conversationId);
    if (!acc || request == std::nullopt) {
        JAMI_WARN("[Account %s] Request not found for conversation %s",
                  accountId_.c_str(),
                  conversationId.c_str());
        return;
    }
    {
        std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
        pendingConversationsFetch_[conversationId] = PendingConversationFetch {};
    }
    auto memberHash = dht::InfoHash(request->from);
    if (!memberHash) {
        JAMI_WARN("Invalid member detected: %s", request->from.c_str());
        return;
    }
    acc->forEachDevice(
        memberHash,
        [this, request = *request, conversationId](
            const std::shared_ptr<dht::crypto::PublicKey>& pk) {
            if (pk->getLongId().toString() == deviceId_)
                return;

            onNeedGitSocket_(conversationId, pk->getLongId().toString(), [=](const auto& channel) {
                auto acc = account_.lock();
                std::unique_lock<std::mutex> lk(pendingConversationsFetchMtx_);
                auto& pending = pendingConversationsFetch_[conversationId];
                if (channel && !pending.ready) {
                    pending.ready = true;
                    pending.deviceId = channel->deviceId().toString();
                    lk.unlock();
                    // Save the git socket
                    acc->addGitSocket(channel->deviceId(),
                                      conversationId,
                                      channel); // TODO move from there
                    checkConversationsEvents();
                    return true;
                }
                return false;
            });
        });
    rmConversationRequest(conversationId);
    ConvInfo info;
    info.id = conversationId;
    info.created = std::time(nullptr);
    info.members.emplace_back(username_);
    info.members.emplace_back(request->from);
    runOnMainThread([w = weak(), info = std::move(info)] {
        if (auto shared = w.lock())
            shared->addConvInfo(info);
    });
}

void
ConversationModule::rmConversationRequest(const std::string& id)
{
    std::lock_guard<std::mutex> lk(conversationsRequestsMtx_);
    conversationsRequests_.erase(id);
    saveConvRequests();
}

void
ConversationModule::onNeedConversationRequest(const std::string& from,
                                              const std::string& conversationId)
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
        sendMsgCb_(from, std::move(invite));
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

bool
ConversationModule::needsSyncingWith(const std::string& memberUri, const std::string& deviceId) const
{
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    for (const auto& [_, conv] : conversations_) {
        if (conv->isMember(memberUri, false) && conv->needsFetch(deviceId)) {
            return true;
        }
    }
    return false;
}

bool
ConversationModule::isBannedDevice(const std::string& convId, const std::string& deviceId) const
{
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(convId);
    return conversation == conversations_.end() || !conversation->second
           || conversation->second->isBanned(deviceId);
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
                    auto conversation = std::make_shared<Conversation>(account_,
                                                                       deviceId,
                                                                       conversationId);
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
                        needsSyncingCb_();
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