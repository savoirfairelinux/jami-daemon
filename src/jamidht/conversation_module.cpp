/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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

#include "account_const.h"
#include "call.h"
#include "client/ring_signal.h"
#include "fileutils.h"
#include "jamidht/account_manager.h"
#include "jamidht/jamiaccount.h"
#include "manager.h"
#include "sip/sipcall.h"
#include "vcard.h"

namespace jami {

using ConvInfoMap = std::map<std::string, ConvInfo>;

struct PendingConversationFetch
{
    bool ready {false};
    bool cloning {false};
    std::string deviceId {};
    std::string removeId {};
    std::map<std::string, std::string> preferences {};
    std::map<std::string, std::string> lastDisplayed {};
    std::set<std::string> connectingTo {};
    std::shared_ptr<ChannelSocket> socket {};
};

class ConversationModule::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::weak_ptr<JamiAccount>&& account,
         NeedsSyncingCb&& needsSyncingCb,
         SengMsgCb&& sendMsgCb,
         NeedSocketCb&& onNeedSocket,
         NeedSocketCb&& onNeedSwarmSocket,
         UpdateConvReq&& updateConvReqCb);

    // Retrieving recent commits
    /**
     * Clone a conversation (initial) from device
     * @param deviceId
     * @param convId
     * @param lastDisplayed      Last message displayed by account
     */
    void cloneConversation(const std::string& deviceId,
                           const std::string& peer,
                           const std::string& convId,
                           const std::string& lastDisplayed = "");

    /**
     * Pull remote device
     * @param peer              Contact URI
     * @param deviceId          Contact's device
     * @param conversationId
     * @param commitId (optional)
     */
    void fetchNewCommits(const std::string& peer,
                         const std::string& deviceId,
                         const std::string& conversationId,
                         const std::string& commitId = "");
    /**
     * Handle events to receive new commits
     */
    void checkConversationsEvents();
    bool handlePendingConversations();
    void handlePendingConversation(const std::string& conversationId, const std::string& deviceId);

    // Requests
    std::optional<ConversationRequest> getRequest(const std::string& id) const;

    // Conversations
    /**
     * Get members
     * @param conversationId
     * @param includeBanned
     * @return a map of members with their role and details
     */
    std::vector<std::map<std::string, std::string>> getConversationMembers(
        const std::string& conversationId, bool includeBanned = false) const;

    /**
     * Remove a repository and all files
     * @param convId
     * @param sync      If we send an update to other account's devices
     * @param force     True if ignore the removing flag
     */
    void removeRepository(const std::string& convId, bool sync, bool force = false);
    /**
     * Remove a conversation
     * @param conversationId
     */
    bool removeConversation(const std::string& conversationId);

    /**
     * Send a message notification to all members
     * @param conversation
     * @param commit
     * @param sync      If we send an update to other account's devices
     */
    void sendMessageNotification(const std::string& conversationId,
                                 const std::string& commitId,
                                 bool sync);
    void sendMessageNotification(const Conversation& conversation,
                                 const std::string& commitId,
                                 bool sync);

    /**
     * @return if a convId is a valid conversation (repository cloned & usable)
     */
    bool isConversation(const std::string& convId) const
    {
        std::lock_guard<std::mutex> lk(conversationsMtx_);
        return conversations_.find(convId) != conversations_.end();
    }
    /**
     * @return if a convId is an accepted conversation
     */
    bool isAcceptedConversation(const std::string& convId) const
    {
        std::lock_guard<std::mutex> lk(conversationsMtx_);
        auto itConv = convInfos_.find(convId);
        return itConv != convInfos_.end() && !itConv->second.removed;
    }

    void addConvInfo(const ConvInfo& info)
    {
        std::lock_guard<std::mutex> lk(convInfosMtx_);
        convInfos_[info.id] = info;
        saveConvInfos();
    }

    /**
     * Updates last displayed for sync infos and client
     */
    void onLastDisplayedUpdated(const std::string& convId, const std::string& lastId)
    {
        // must not lock as used in callback from a conversation,
        // so convInfos_ cannot change for convId
        auto itConv = convInfos_.find(convId);
        if (itConv != convInfos_.end())
            itConv->second.lastDisplayed = lastId;
        saveConvInfos();

        // Updates info for client
        emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
            accountId_,
            convId,
            username_,
            lastId,
            static_cast<int>(libjami::Account::MessageStates::DISPLAYED));
    }

    std::string getOneToOneConversation(const std::string& uri) const noexcept;

    std::weak_ptr<JamiAccount> account_;
    NeedsSyncingCb needsSyncingCb_;
    SengMsgCb sendMsgCb_;
    NeedSocketCb onNeedSocket_;
    NeedSocketCb onNeedSwarmSocket_;
    UpdateConvReq updateConvReqCb_;

    std::string accountId_ {};
    std::string deviceId_ {};
    std::string username_ {};

    // Requests
    mutable std::mutex conversationsRequestsMtx_;
    std::map<std::string, ConversationRequest> conversationsRequests_;

    // Conversations
    mutable std::mutex conversationsMtx_ {};
    std::map<std::string, std::shared_ptr<Conversation>, std::less<>> conversations_;
    std::mutex pendingConversationsFetchMtx_ {};
    std::map<std::string, PendingConversationFetch, std::less<>> pendingConversationsFetch_;

    bool startFetch(const std::string& convId, const std::string& deviceId)
    {
        std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
        auto it = pendingConversationsFetch_.find(convId);
        if (it == pendingConversationsFetch_.end()) {
            PendingConversationFetch pf;
            pf.connectingTo.insert(deviceId);
            pendingConversationsFetch_[convId] = std::move(pf);
            return true;
        }
        auto& pf = it->second;
        if (pf.ready)
            return false; // Already doing stuff
        if (pf.connectingTo.find(deviceId) != pf.connectingTo.end())
            return false; // Already connecting to this device
        pf.connectingTo.insert(deviceId);
        return true;
    }

    void stopFetch(const std::string& convId, const std::string& deviceId)
    {
        auto it = pendingConversationsFetch_.find(convId);
        if (it == pendingConversationsFetch_.end())
            return;
        auto& pf = it->second;
        pf.connectingTo.erase(deviceId);
        if (pf.connectingTo.empty())
            pendingConversationsFetch_.erase(it);
    }

    // Message send/load
    void sendMessage(const std::string& conversationId,
                     Json::Value&& value,
                     const std::string& replyTo = "",
                     bool announce = true,
                     OnCommitCb&& onCommit = {},
                     OnDoneCb&& cb = {});

    void sendMessage(const std::string& conversationId,
                     std::string message,
                     const std::string& replyTo = "",
                     const std::string& type = "text/plain",
                     bool announce = true,
                     OnCommitCb&& onCommit = {},
                     OnDoneCb&& cb = {});

    void editMessage(const std::string& conversationId,
                     const std::string& newBody,
                     const std::string& editedId);

    // The following informations are stored on the disk
    mutable std::mutex convInfosMtx_; // Note, should be locked after conversationsMtx_ if needed
    std::map<std::string, ConvInfo> convInfos_;
    // The following methods modify what is stored on the disk
    /**
     * @note convInfosMtx_ should be locked
     */
    void saveConvInfos() const { ConversationModule::saveConvInfos(accountId_, convInfos_); }
    /**
     * @note conversationsRequestsMtx_ should be locked
     */
    void saveConvRequests() const
    {
        ConversationModule::saveConvRequests(accountId_, conversationsRequests_);
    }
    bool addConversationRequest(const std::string& id, const ConversationRequest& req)
    {
        std::lock_guard<std::mutex> lk(conversationsRequestsMtx_);
        auto it = conversationsRequests_.find(id);
        if (it != conversationsRequests_.end()) {
            // Check if updated
            if (req == it->second)
                return false;
        }
        conversationsRequests_[id] = req;
        saveConvRequests();
        return true;
    }
    void rmConversationRequest(const std::string& id)
    {
        std::lock_guard<std::mutex> lk(conversationsRequestsMtx_);
        conversationsRequests_.erase(id);
        saveConvRequests();
    }

    // Receiving new commits
    std::shared_ptr<RepeatedTask> conversationsEventHandler {};

    std::weak_ptr<Impl> weak() { return std::static_pointer_cast<Impl>(shared_from_this()); }

    // Replay conversations (after erasing/re-adding)
    std::mutex replayMtx_;
    std::map<std::string, std::vector<std::map<std::string, std::string>>> replay_;
    std::map<std::string, uint64_t> refreshMessage;
    std::atomic_int syncCnt {0};
};

ConversationModule::Impl::Impl(std::weak_ptr<JamiAccount>&& account,
                               NeedsSyncingCb&& needsSyncingCb,
                               SengMsgCb&& sendMsgCb,
                               NeedSocketCb&& onNeedSocket,
                               NeedSocketCb&& onNeedSwarmSocket,
                               UpdateConvReq&& updateConvReqCb)
    : account_(account)
    , needsSyncingCb_(needsSyncingCb)
    , sendMsgCb_(sendMsgCb)
    , onNeedSocket_(onNeedSocket)
    , onNeedSwarmSocket_(onNeedSwarmSocket)
    , updateConvReqCb_(updateConvReqCb)
{
    if (auto shared = account.lock()) {
        accountId_ = shared->getAccountID();
        deviceId_ = shared->currentDeviceId();
        if (const auto* accm = shared->accountManager())
            if (const auto* info = accm->getInfo())
                username_ = info->accountId;
    }
    conversationsRequests_ = convRequests(accountId_);
}

void
ConversationModule::Impl::cloneConversation(const std::string& deviceId,
                                            const std::string& peerUri,
                                            const std::string& convId,
                                            const std::string& lastDisplayed)
{
    JAMI_DBG("[Account %s] Clone conversation on device %s", accountId_.c_str(), deviceId.c_str());

    if (!isConversation(convId)) {
        // Note: here we don't return and connect to all members
        // the first that will successfully connect will be used for
        // cloning.
        // This avoid the case when we try to clone from convInfos + sync message
        // at the same time.
        if (!startFetch(convId, deviceId)) {
            JAMI_WARN("[Account %s] Already fetching %s", accountId_.c_str(), convId.c_str());
            std::lock_guard<std::mutex> lk(convInfosMtx_);
            auto ci = convInfos_.find(convId);
            if (ci != convInfos_.end() && ci->second.lastDisplayed.empty()) {
                // If fetchNewCommits called before sync
                ci->second.lastDisplayed = lastDisplayed;
                saveConvInfos();
            }
            return;
        }
        onNeedSocket_(
            convId,
            deviceId,
            [=](const auto& channel) {
                auto acc = account_.lock();
                std::unique_lock<std::mutex> lk(pendingConversationsFetchMtx_);
                auto& pending = pendingConversationsFetch_[convId];
                if (!pending.ready) {
                    if (channel) {
                        pending.ready = true;
                        pending.deviceId = channel->deviceId().toString();
                        pending.socket = channel;
                        lk.unlock();
                        checkConversationsEvents();
                        return true;
                    } else {
                        stopFetch(convId, deviceId);
                    }
                }
                return false;
            },
            "application/im-gitmessage-id");

        JAMI_INFO("[Account %s] New conversation detected: %s. Ask device %s to clone it",
                  accountId_.c_str(),
                  convId.c_str(),
                  deviceId.c_str());
        ConvInfo info;
        info.id = convId;
        info.created = std::time(nullptr);
        info.members.emplace_back(username_);
        info.lastDisplayed = lastDisplayed;
        if (peerUri != username_)
            info.members.emplace_back(peerUri);

        std::lock_guard<std::mutex> lk(convInfosMtx_);
        convInfos_[info.id] = std::move(info);
        saveConvInfos();
    } else {
        std::unique_lock<std::mutex> lk(conversationsMtx_);
        auto conversation = conversations_.find(convId);
        if (conversation != conversations_.end() && conversation->second)
            conversation->second->updateLastDisplayed(lastDisplayed);
        JAMI_INFO("[Account %s] Already have conversation %s", accountId_.c_str(), convId.c_str());
    }
}

void
ConversationModule::Impl::fetchNewCommits(const std::string& peer,
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

        if (!startFetch(conversationId, deviceId)) {
            JAMI_WARN("[Account %s] Already fetching %s",
                      accountId_.c_str(),
                      conversationId.c_str());
            return;
        }
        syncCnt.fetch_add(1);
        onNeedSocket_(
            conversationId,
            deviceId,
            [this,
             conversationId = std::move(conversationId),
             peer = std::move(peer),
             deviceId = std::move(deviceId),
             commitId = std::move(commitId)](const auto& channel) {
                auto conversation = conversations_.find(conversationId);
                auto acc = account_.lock();
                if (!channel || !acc || conversation == conversations_.end()
                    || !conversation->second) {
                    std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
                    stopFetch(conversationId, deviceId);
                    syncCnt.fetch_sub(1);
                    return false;
                }
                conversation->second->addGitSocket(channel->deviceId(), channel);
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
                        }
                        {
                            std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
                            pendingConversationsFetch_.erase(conversationId);
                        }
                        if (syncCnt.fetch_sub(1) == 1) {
                            if (auto account = account_.lock())
                                emitSignal<libjami::ConversationSignal::ConversationSyncFinished>(
                                    account->getAccountID().c_str());
                        }
                    },
                    commitId);
                return true;
            },
            "");
    } else {
        if (getRequest(conversationId) != std::nullopt)
            return;
        {
            // Check if the conversation is cloning
            std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
            if (pendingConversationsFetch_.find(conversationId) != pendingConversationsFetch_.end())
                return;
        }
        bool clone = false;
        {
            std::lock_guard<std::mutex> lkCi(convInfosMtx_);
            auto convIt = convInfos_.find(conversationId);
            clone = convIt != convInfos_.end();
        }
        lk.unlock();
        if (clone) {
            cloneConversation(deviceId, peer, conversationId);
            return;
        }
        JAMI_WARN("[Account %s] Could not find conversation %s, ask for an invite",
                  accountId_.c_str(),
                  conversationId.c_str());
        sendMsgCb_(peer,
                   std::map<std::string, std::string> {{"application/invite", conversationId}},
                   0);
    }
}

void
ConversationModule::Impl::checkConversationsEvents()
{
    std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
    bool hasHandler = conversationsEventHandler and not conversationsEventHandler->isCancelled();
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

// Clone and store conversation
void
ConversationModule::Impl::handlePendingConversation(const std::string& conversationId,
                                                    const std::string& deviceId)
{
    auto erasePending = [&] {
        std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
        auto oldFetch = pendingConversationsFetch_.find(conversationId);
        if (oldFetch != pendingConversationsFetch_.end() && !oldFetch->second.removeId.empty())
            removeConversation(oldFetch->second.removeId);
        pendingConversationsFetch_.erase(conversationId);
    };
    try {
        auto conversation = std::make_shared<Conversation>(account_, deviceId, conversationId);
        {
            std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
            auto oldFetch = pendingConversationsFetch_.find(conversationId);
            if (oldFetch != pendingConversationsFetch_.end() && oldFetch->second.socket)
                conversation->addGitSocket(DeviceId(deviceId), std::move(oldFetch->second.socket));
        }
        conversation->onLastDisplayedUpdated(
            [&](auto convId, auto lastId) { onLastDisplayedUpdated(convId, lastId); });
        if (!conversation->isMember(username_, true)) {
            JAMI_ERR("Conversation cloned but doesn't seems to be a valid member");
            conversation->erase();
            erasePending();
            return;
        }
        conversation->onNeedSocket(onNeedSwarmSocket_);
        auto removeRepo = false;

        {
            std::lock_guard<std::mutex> lk(conversationsMtx_);
            // Note: a removeContact while cloning. In this case, the conversation
            // must not be announced and removed.
            auto itConv = convInfos_.find(conversationId);
            if (itConv != convInfos_.end() && itConv->second.removed)
                removeRepo = true;
            if (itConv != convInfos_.end() && !itConv->second.lastDisplayed.empty()) {
                conversation->updateLastDisplayed(itConv->second.lastDisplayed);
            }
            conversations_.emplace(conversationId, conversation);
        }
        if (removeRepo) {
            removeRepository(conversationId, false, true);
            erasePending();
            return;
        }
        auto commitId = conversation->join();
        std::vector<std::map<std::string, std::string>> messages;
        {
            std::lock_guard<std::mutex> lk(replayMtx_);
            auto replayIt = replay_.find(conversationId);
            if (replayIt != replay_.end()) {
                messages = std::move(replayIt->second);
                replay_.erase(replayIt);
            }
        }
        if (!commitId.empty())
            sendMessageNotification(conversationId, false, commitId);
        std::map<std::string, std::string> preferences;
        std::map<std::string, std::string> lastDisplayed;
        {
            std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
            auto itFetch = pendingConversationsFetch_.find(conversationId);
            if (itFetch != pendingConversationsFetch_.end()) {
                preferences = std::move(itFetch->second.preferences);
                lastDisplayed = std::move(itFetch->second.lastDisplayed);
            }
        }
        if (!preferences.empty())
            conversation->updatePreferences(preferences);
        if (!lastDisplayed.empty())
            conversation->updateLastDisplayed(lastDisplayed);
        // Inform user that the conversation is ready
        emitSignal<libjami::ConversationSignal::ConversationReady>(accountId_, conversationId);
        needsSyncingCb_({});
        std::vector<Json::Value> values;
        values.reserve(messages.size());
        for (const auto& message : messages) {
            // For now, only replay text messages.
            // File transfers will need more logic, and don't care about calls for now.
            if (message.at("type") == "text/plain" && message.at("author") == username_) {
                Json::Value json;
                json["body"] = message.at("body");
                json["type"] = "text/plain";
                values.emplace_back(std::move(json));
            }
        }
        if (!values.empty())
            conversation->sendMessages(std::move(values),
                                       [w = weak(), conversationId](const auto& commits) {
                                           auto shared = w.lock();
                                           if (shared and not commits.empty())
                                               shared->sendMessageNotification(conversationId,
                                                                               *commits.rbegin(),
                                                                               true);
                                       });
        // Download members profile on first sync
        auto isOneOne = conversation->mode() == ConversationMode::ONE_TO_ONE;
        auto askForProfile = isOneOne;
        if (!isOneOne) {
            // If not 1:1 only download profiles from self (to avoid non checked files)
            auto cert = tls::CertificateStore::instance().getCertificate(deviceId);
            askForProfile = cert && cert->issuer && cert->issuer->getId().toString() == username_;
        }
        if (askForProfile) {
            for (const auto& member : conversation->memberUris(username_)) {
                acc->askForProfile(conversationId, deviceId, member);
            }
        }
    } catch (const std::exception& e) {
        JAMI_WARN("Something went wrong when cloning conversation: %s", e.what());
    }
    erasePending();
}

bool
ConversationModule::Impl::handlePendingConversations()
{
    std::lock_guard<std::mutex> lk(pendingConversationsFetchMtx_);
    for (auto it = pendingConversationsFetch_.begin(); it != pendingConversationsFetch_.end();) {
        if (it->second.ready && !it->second.cloning) {
            it->second.cloning = true;
            dht::ThreadPool::io().run(
                [w = weak(), conversationId = it->first, deviceId = it->second.deviceId]() {
                    if (auto sthis = w.lock())
                        sthis->handlePendingConversation(conversationId, deviceId);
                });
        }
        ++it;
    }
    return !pendingConversationsFetch_.empty();
}

std::optional<ConversationRequest>
ConversationModule::Impl::getRequest(const std::string& id) const
{
    std::lock_guard<std::mutex> lk(conversationsRequestsMtx_);
    auto it = conversationsRequests_.find(id);
    if (it != conversationsRequests_.end())
        return it->second;
    return std::nullopt;
}

std::string
ConversationModule::Impl::getOneToOneConversation(const std::string& uri) const noexcept
{
    auto acc = account_.lock();
    if (!acc)
        return {};
    auto details = acc->getContactDetails(uri);
    auto it = details.find(libjami::Account::TrustRequest::CONVERSATIONID);
    if (it != details.end())
        return it->second;
    return {};
}

std::vector<std::map<std::string, std::string>>
ConversationModule::Impl::getConversationMembers(const std::string& conversationId, bool includeBanned) const
{
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(conversationId);
    if (conversation != conversations_.end() && conversation->second)
        return conversation->second->getMembers(true, true, includeBanned);

    lk.unlock();
    std::lock_guard<std::mutex> lkCI(convInfosMtx_);
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

void
ConversationModule::Impl::removeRepository(const std::string& conversationId, bool sync, bool force)
{
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    auto it = conversations_.find(conversationId);
    if (it != conversations_.end() && it->second && (force || it->second->isRemoving())) {
        JAMI_DBG() << "Remove conversation: " << conversationId;
        try {
            if (it->second->mode() == ConversationMode::ONE_TO_ONE) {
                auto account = account_.lock();
                for (const auto& member : it->second->getInitialMembers()) {
                    if (member != account->getUsername()) {
                        // Note: this can happen while re-adding a contact.
                        // In this case, check that we are removing the linked conversation.
                        if (conversationId == getOneToOneConversation(member)) {
                            account->accountManager()->removeContactConversation(member);
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            JAMI_ERR() << e.what();
        }
        it->second->erase();
        conversations_.erase(it);
        lk.unlock();

        if (!sync)
            return;
        std::lock_guard<std::mutex> lkCi(convInfosMtx_);
        auto convIt = convInfos_.find(conversationId);
        if (convIt != convInfos_.end()) {
            convIt->second.erased = std::time(nullptr);
            needsSyncingCb_({});
        }
        saveConvInfos();
    }
}

bool
ConversationModule::Impl::removeConversation(const std::string& conversationId)
{
    auto members = getConversationMembers(conversationId);
    std::unique_lock<std::mutex> lk(conversationsMtx_);
    // Update convInfos
    std::unique_lock<std::mutex> lockCi(convInfosMtx_);
    auto itConv = convInfos_.find(conversationId);
    if (itConv == convInfos_.end()) {
        JAMI_ERR("Conversation %s doesn't exist", conversationId.c_str());
        return false;
    }
    auto it = conversations_.find(conversationId);
    auto isSyncing = it == conversations_.end();
    auto hasMembers = !isSyncing && !(members.size() == 1 && username_ == members[0]["uri"]);
    itConv->second.removed = std::time(nullptr);
    if (isSyncing)
        itConv->second.erased = std::time(nullptr);
    // Sync now, because it can take some time to really removes the datas
    if (hasMembers)
        needsSyncingCb_({});
    saveConvInfos();
    lockCi.unlock();
    emitSignal<libjami::ConversationSignal::ConversationRemoved>(accountId_, conversationId);
    if (isSyncing)
        return true;
    if (it->second->mode() != ConversationMode::ONE_TO_ONE) {
        // For one to one, we do not notify the leave. The other can still generate request
        // and this is managed by the banned part. If we re-accept, the old conversation will be
        // retrieved
        auto commitId = it->second->leave();
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
            // peer that we left the conversation and never want to receive
            // any messages
            return true;
        }
    } else {
        for (const auto& m : members)
            if (username_ != m.at("uri") && updateConvReqCb_)
                updateConvReqCb_(conversationId, m.at("uri"), false);
    }
    lk.unlock();
    // Else we are the last member, so we can remove
    removeRepository(conversationId, true);
    return true;
}

void
ConversationModule::Impl::sendMessageNotification(const std::string& conversationId,
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
ConversationModule::Impl::sendMessageNotification(const Conversation& conversation,
                                                  const std::string& commitId,
                                                  bool sync)
{
    Json::Value message;
    message["id"] = conversation.id();
    message["commit"] = commitId;
    message["deviceId"] = deviceId_;
    Json::StreamWriterBuilder builder;
    const auto text = Json::writeString(builder, message);
    for (const auto& member : conversation.memberUris(sync ? "" : username_)) {
        // Announce to all members that a new message is sent
        refreshMessage[member] = sendMsgCb_(member,
                                            std::map<std::string, std::string> {
                                                {"application/im-gitmessage-id", text}},
                                            refreshMessage[member]);
    }
}

void
ConversationModule::Impl::sendMessage(const std::string& conversationId,
                                      std::string message,
                                      const std::string& replyTo,
                                      const std::string& type,
                                      bool announce,
                                      OnCommitCb&& onCommit,
                                      OnDoneCb&& cb)
{
    Json::Value json;
    json["body"] = std::move(message);
    json["type"] = type;
    sendMessage(conversationId,
                std::move(json),
                replyTo,
                announce,
                std::move(onCommit),
                std::move(cb));
}

void
ConversationModule::Impl::sendMessage(const std::string& conversationId,
                                      Json::Value&& value,
                                      const std::string& replyTo,
                                      bool announce,
                                      OnCommitCb&& onCommit,
                                      OnDoneCb&& cb)
{
    std::lock_guard<std::mutex> lk(conversationsMtx_);
    auto conversation = conversations_.find(conversationId);
    if (conversation != conversations_.end() && conversation->second) {
        conversation->second->sendMessage(
            std::move(value),
            replyTo,
            std::move(onCommit),
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
ConversationModule::Impl::editMessage(const std::string& conversationId,
                                      const std::string& newBody,
                                      const std::string& editedId)
{
    // Check that editedId is a valid commit, from ourself and plain/text
    auto validCommit = false;
    {
        std::lock_guard<std::mutex> lk(conversationsMtx_);
        auto conversation = conversations_.find(conversationId);
        if (conversation != conversations_.end() && conversation->second) {
            auto commit = conversation->second->getCommit(editedId);
            if (commit != std::nullopt) {
                validCommit = commit->at("author") == username_
                              && commit->at("type") == "text/plain";
            }
        }
    }
    if (!validCommit) {
        JAMI_ERROR("Cannot edit commit {:s}", editedId);
        return;
    }
    // Commit message edition
    Json::Value json;
    json["body"] = newBody;
    json["edit"] = editedId;
    json["type"] = "application/edited-message";
    sendMessage(conversationId, std::move(json));
}

////////////////////////////////////////////////////////////////

void
ConversationModule::saveConvRequests(
    const std::string& accountId,
    const std::map<std::string, ConversationRequest>& conversationsRequests)
{
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId;
    saveConvRequestsToPath(path, conversationsRequests);
}

void
ConversationModule::saveConvRequestsToPath(
    const std::string& path, const std::map<std::string, ConversationRequest>& conversationsRequests)
{
    std::lock_guard<std::mutex> lock(
        fileutils::getFileLock(path + DIR_SEPARATOR_STR + "convRequests"));
    std::ofstream file(path + DIR_SEPARATOR_STR + "convRequests",
                       std::ios::trunc | std::ios::binary);
    msgpack::pack(file, conversationsRequests);
}

void
ConversationModule::saveConvInfos(const std::string& accountId, const ConvInfoMap& conversations)
{
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId;
    saveConvInfosToPath(path, conversations);
}

void
ConversationModule::saveConvInfosToPath(const std::string& path, const ConvInfoMap& conversations)
{
    std::ofstream file(path + DIR_SEPARATOR_STR + "convInfo", std::ios::trunc | std::ios::binary);
    msgpack::pack(file, conversations);
}

////////////////////////////////////////////////////////////////

ConversationModule::ConversationModule(std::weak_ptr<JamiAccount>&& account,
                                       NeedsSyncingCb&& needsSyncingCb,
                                       SengMsgCb&& sendMsgCb,
                                       NeedSocketCb&& onNeedSocket,
                                       NeedSocketCb&& onNeedSwarmSocket,
                                       UpdateConvReq&& updateConvReqCb)
    : pimpl_ {std::make_unique<Impl>(std::move(account),
                                     std::move(needsSyncingCb),
                                     std::move(sendMsgCb),
                                     std::move(onNeedSocket),
                                     std::move(onNeedSwarmSocket),
                                     std::move(updateConvReqCb))}
{
    loadConversations();
}

void
ConversationModule::loadConversations()
{
    auto acc = pimpl_->account_.lock();
    if (!acc)
        return;
    auto uri = acc->getUsername();
    JAMI_INFO("[Account %s] Start loading conversations…", pimpl_->accountId_.c_str());
    auto conversationsRepositories = fileutils::readDirectory(
        fileutils::get_data_dir() + DIR_SEPARATOR_STR + pimpl_->accountId_ + DIR_SEPARATOR_STR
        + "conversations");
    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    pimpl_->convInfos_ = convInfos(pimpl_->accountId_);
    pimpl_->conversations_.clear();
    std::set<std::string> toRm;
    for (const auto& repository : conversationsRepositories) {
        try {
            auto conv = std::make_shared<Conversation>(pimpl_->account_, repository);
            conv->onLastDisplayedUpdated(
                [&](auto convId, auto lastId) { pimpl_->onLastDisplayedUpdated(convId, lastId); });
            conv->onNeedSocket(pimpl_->onNeedSwarmSocket_);
            auto members = conv->memberUris(uri, {});
            // NOTE: The following if is here to protect against any incorrect state
            // that can be introduced
            if (conv->mode() == ConversationMode::ONE_TO_ONE && members.size() == 1) {
                // If we got a 1:1 conversation, but not in the contact details, it's rather a
                // duplicate or a weird state
                auto& otherUri = members[0];
                auto convFromDetails = getOneToOneConversation(otherUri);
                if (convFromDetails != repository) {
                    if (convFromDetails.empty()) {
                        JAMI_ERROR(
                            "No conversation detected for {} but one exists ({}). Update details",
                            otherUri,
                            repository);
                        acc->updateConvForContact(otherUri, convFromDetails, repository);
                    } else {
                        JAMI_ERROR("Multiple conversation detected for {} but ({} & {})",
                                   otherUri,
                                   repository,
                                   convFromDetails);
                        toRm.insert(repository);
                    }
                }
            }
            conv->onLastDisplayedUpdated(
                [&](auto convId, auto lastId) { pimpl_->onLastDisplayedUpdated(convId, lastId); });
            auto convInfo = pimpl_->convInfos_.find(repository);
            if (convInfo == pimpl_->convInfos_.end()) {
                JAMI_ERR() << "Missing conv info for " << repository << ". This is a bug!";
                ConvInfo info;
                info.id = repository;
                info.created = std::time(nullptr);
                info.members = std::move(members);
                info.lastDisplayed = conv->infos()[ConversationMapKeys::LAST_DISPLAYED];
                addConvInfo(info);
            }
            auto commits = conv->refreshActiveCalls();
            if (!commits.empty()) {
                // Note: here, this means that some calls were actives while the
                // daemon finished (can be a crash).
                // Notify other in the conversation that the call is finished
                pimpl_->sendMessageNotification(*conv, *commits.rbegin(), true);
            }
            pimpl_->conversations_.emplace(repository, std::move(conv));
        } catch (const std::logic_error& e) {
            JAMI_WARN("[Account %s] Conversations not loaded : %s",
                      pimpl_->accountId_.c_str(),
                      e.what());
        }
    }

    // Prune any invalid conversations without members and
    // set the removed flag if needed
    size_t oldConvInfosSize = pimpl_->convInfos_.size();
    std::set<std::string> removed;
    for (auto itInfo = pimpl_->convInfos_.begin(); itInfo != pimpl_->convInfos_.end();) {
        const auto& info = itInfo->second;
        if (info.members.empty()) {
            itInfo = pimpl_->convInfos_.erase(itInfo);
            continue;
        }
        if (info.removed)
            removed.insert(info.id);
        auto itConv = pimpl_->conversations_.find(info.id);
        if (itConv != pimpl_->conversations_.end() && info.removed)
            itConv->second->setRemovingFlag();
        if (!info.removed && itConv == pimpl_->conversations_.end()) {
            // In this case, the conversation is not synced and we only know ourself
            if (info.members.size() == 1 && info.members.at(0) == uri) {
                JAMI_WARNING("[Account {:s}] Conversation {:s} seems not present/synced.",
                             pimpl_->accountId_,
                             info.id);
                emitSignal<libjami::ConversationSignal::ConversationRemoved>(pimpl_->accountId_,
                                                                             info.id);
                itInfo = pimpl_->convInfos_.erase(itInfo);
                continue;
            }
        }
        ++itInfo;
    }
    // On oldest version, removeConversation didn't update "appdata/contacts"
    // causing a potential incorrect state between "appdata/contacts" and "appdata/convInfos"
    if (!removed.empty())
        acc->unlinkConversations(removed);
    // Save if we've removed some invalid entries
    if (oldConvInfosSize != pimpl_->convInfos_.size())
        pimpl_->saveConvInfos();

    lk.unlock();

    ////////////////////////////////////////////////////////////////
    // Note: This is only to homogeneize trust and convRequests
    std::vector<std::string> invalidPendingRequests;
    {
        std::lock_guard<std::mutex> lk(pimpl_->conversationsRequestsMtx_);
        for (const auto& request: acc->getTrustRequests()) {
            auto itConvId = request.find(libjami::Account::TrustRequest::CONVERSATIONID);
            auto itConvFrom = request.find(libjami::Account::TrustRequest::FROM);
            if (itConvId != request.end() && itConvFrom != request.end())
            {
                // Check if requests exists or is declined.
                auto itReq = pimpl_->conversationsRequests_.find(itConvId->second);
                auto declined = itReq  == pimpl_->conversationsRequests_.end() || itReq->second.declined;
                if (declined) {
                    JAMI_WARNING("Invalid trust request found: {:s}", itConvId->second);
                    invalidPendingRequests.emplace_back(itConvFrom->second);
                }
            }
        }
    }
    dht::ThreadPool::io().run([w=pimpl_->weak(), invalidPendingRequests = std::move(invalidPendingRequests)] () {
        // Will lock account manager
        auto shared = w.lock();
        if (!shared)
            return;
        if (auto acc = shared->account_.lock()) {
            for (const auto& invalidPendingRequest : invalidPendingRequests)
                acc->discardTrustRequest(invalidPendingRequest);
        }
    });

    ////////////////////////////////////////////////////////////////
    for (const auto& conv : toRm) {
        JAMI_ERROR("Remove conversation ({})", conv);
        removeConversation(conv);
    }
    JAMI_INFO("[Account %s] Conversations loaded!", pimpl_->accountId_.c_str());
}

void
ConversationModule::clearPendingFetch()
{
    if (!pimpl_->pendingConversationsFetch_.empty()) {
        // Note: This is a fallback. convModule() is kept if account is disabled/re-enabled.
        // iOS uses setAccountActive() a lot, and if for some reason the previous pending fetch is
        // not erased (callback not called), it will block the new messages as it will not sync. The
        // best way to debug this is to get logs from the last ICE connection for syncing the
        // conversation. It may have been killed in some un-expected way avoiding to call the
        // callbacks. This should never happen, but if it's the case, this will allow new messages
        // to be synced correctly.
        JAMI_ERR("This is a bug, seems to still fetch to some device on initializing");
        pimpl_->pendingConversationsFetch_.clear();
    }
}

std::vector<std::string>
ConversationModule::getConversations() const
{
    std::vector<std::string> result;
    std::lock_guard<std::mutex> lk(pimpl_->convInfosMtx_);
    result.reserve(pimpl_->convInfos_.size());
    for (const auto& [key, conv] : pimpl_->convInfos_) {
        if (conv.removed)
            continue;
        result.emplace_back(key);
    }
    return result;
}

std::string
ConversationModule::getOneToOneConversation(const std::string& uri) const noexcept
{
    return pimpl_->getOneToOneConversation(uri);
}

std::vector<std::map<std::string, std::string>>
ConversationModule::getConversationRequests() const
{
    std::vector<std::map<std::string, std::string>> requests;
    std::lock_guard<std::mutex> lk(pimpl_->conversationsRequestsMtx_);
    requests.reserve(pimpl_->conversationsRequests_.size());
    for (const auto& [id, request] : pimpl_->conversationsRequests_) {
        if (request.declined)
            continue; // Do not add declined requests
        requests.emplace_back(request.toMap());
    }
    return requests;
}

void
ConversationModule::onTrustRequest(const std::string& uri,
                                   const std::string& conversationId,
                                   const std::vector<uint8_t>& payload,
                                   time_t received)
{
    if (getOneToOneConversation(uri) != "") {
        // If there is already an active one to one conversation here, it's an active
        // contact and the contact will reclone this activeConv, so ignore the request
        JAMI_WARN("Contact is sending a request for a non active conversation. Ignore. They will "
                  "clone the old one");
        return;
    }
    if (pimpl_->isAcceptedConversation(conversationId)) {
        JAMI_INFO("[Account %s] Received a request for a conversation "
                    "already handled. Ignore",
                    pimpl_->accountId_.c_str());
        return;
    }
    if (pimpl_->getRequest(conversationId) != std::nullopt) {
        JAMI_INFO("[Account %s] Received a request for a conversation "
                  "already existing. Ignore",
                  pimpl_->accountId_.c_str());
        return;
    }
    emitSignal<libjami::ConfigurationSignal::IncomingTrustRequest>(pimpl_->accountId_,
                                                                   conversationId,
                                                                   uri,
                                                                   payload,
                                                                   received);
    ConversationRequest req;
    req.from = uri;
    req.conversationId = conversationId;
    req.received = std::time(nullptr);
    req.metadatas = ConversationRepository::infosFromVCard(vCard::utils::toMap(
        std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size())));
    auto reqMap = req.toMap();
    pimpl_->addConversationRequest(conversationId, std::move(req));
    emitSignal<libjami::ConversationSignal::ConversationRequestReceived>(pimpl_->accountId_,
                                                                         conversationId,
                                                                         reqMap);
}

void
ConversationModule::onConversationRequest(const std::string& from, const Json::Value& value)
{
    ConversationRequest req(value);
    JAMI_INFO("[Account %s] Receive a new conversation request for conversation %s from %s",
              pimpl_->accountId_.c_str(),
              req.conversationId.c_str(),
              from.c_str());
    auto convId = req.conversationId;
    req.from = from;

    // Already accepted request, do nothing
    if (pimpl_->isAcceptedConversation(convId))
        return;
    if (pimpl_->getRequest(convId) != std::nullopt) {
        JAMI_INFO("[Account %s] Received a request for a conversation already existing. "
                  "Ignore",
                  pimpl_->accountId_.c_str());
        return;
    }
    req.received = std::time(nullptr);
    auto reqMap = req.toMap();
    pimpl_->addConversationRequest(convId, std::move(req));
    // Note: no need to sync here because other connected devices should receive
    // the same conversation request. Will sync when the conversation will be added

    emitSignal<libjami::ConversationSignal::ConversationRequestReceived>(pimpl_->accountId_,
                                                                         convId,
                                                                         reqMap);
}

std::string
ConversationModule::peerFromConversationRequest(const std::string& convId) const
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsRequestsMtx_);
    auto it = pimpl_->conversationsRequests_.find(convId);
    if (it != pimpl_->conversationsRequests_.end()) {
        return it->second.from;
    }
    return {};
}

void
ConversationModule::onNeedConversationRequest(const std::string& from,
                                              const std::string& conversationId)
{
    // Check if the conversation exists
    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    auto itConv = pimpl_->conversations_.find(conversationId);
    if (itConv != pimpl_->conversations_.end() && !itConv->second->isRemoving()) {
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
        pimpl_->sendMsgCb_(from, std::move(invite), 0);
    }
}

void
ConversationModule::acceptConversationRequest(const std::string& conversationId)
{
    // For all conversation members, try to open a git channel with this conversation ID
    auto request = pimpl_->getRequest(conversationId);
    if (request == std::nullopt) {
        JAMI_WARN("[Account %s] Request not found for conversation %s",
                  pimpl_->accountId_.c_str(),
                  conversationId.c_str());
        return;
    }
    pimpl_->rmConversationRequest(conversationId);
    if (pimpl_->updateConvReqCb_)
        pimpl_->updateConvReqCb_(conversationId, request->from, true);
    cloneConversationFrom(conversationId, request->from);
}

void
ConversationModule::declineConversationRequest(const std::string& conversationId)
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsRequestsMtx_);
    auto it = pimpl_->conversationsRequests_.find(conversationId);
    if (it != pimpl_->conversationsRequests_.end()) {
        it->second.declined = std::time(nullptr);
        pimpl_->saveConvRequests();
    }
    emitSignal<libjami::ConversationSignal::ConversationRequestDeclined>(pimpl_->accountId_,
                                                                         conversationId);
    pimpl_->needsSyncingCb_({});
}

std::string
ConversationModule::startConversation(ConversationMode mode, const std::string& otherMember)
{
    // Create the conversation object
    std::shared_ptr<Conversation> conversation;
    try {
        conversation = std::make_shared<Conversation>(pimpl_->account_, mode, otherMember);
        conversation->onLastDisplayedUpdated(
            [&](auto convId, auto lastId) { pimpl_->onLastDisplayedUpdated(convId, lastId); });
        conversation->onNeedSocket(pimpl_->onNeedSwarmSocket_);
    } catch (const std::exception& e) {
        JAMI_ERR("[Account %s] Error while generating a conversation %s",
                 pimpl_->accountId_.c_str(),
                 e.what());
        return {};
    }
    auto convId = conversation->id();
    {
        std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
        pimpl_->conversations_[convId] = std::move(conversation);
    }

    // Update convInfo
    ConvInfo info;
    info.id = convId;
    info.created = std::time(nullptr);
    info.members.emplace_back(pimpl_->username_);
    if (!otherMember.empty())
        info.members.emplace_back(otherMember);
    addConvInfo(info);

    pimpl_->needsSyncingCb_({});

    emitSignal<libjami::ConversationSignal::ConversationReady>(pimpl_->accountId_, convId);
    return convId;
}

void
ConversationModule::cloneConversationFrom(const std::string& conversationId,
                                          const std::string& uri,
                                          const std::string& oldConvId)
{
    auto acc = pimpl_->account_.lock();
    auto memberHash = dht::InfoHash(uri);
    if (!acc || !memberHash) {
        JAMI_WARN("Invalid member detected: %s", uri.c_str());
        return;
    }
    acc->forEachDevice(memberHash,
                       [w = pimpl_->weak(), conversationId, oldConvId](
                           const std::shared_ptr<dht::crypto::PublicKey>& pk) {
                           auto sthis = w.lock();
                           auto deviceId = pk->getLongId().toString();
                           if (!sthis or deviceId == sthis->deviceId_)
                               return;
                           if (!sthis->startFetch(conversationId, deviceId)) {
                               JAMI_WARN("[Account %s] Already fetching %s",
                                         sthis->accountId_.c_str(),
                                         conversationId.c_str());
                               return;
                           }
                           sthis->onNeedSocket_(
                               conversationId,
                               pk->getLongId().toString(),
                               [=](const auto& channel) {
                                   auto acc = sthis->account_.lock();
                                   std::unique_lock<std::mutex> lk(
                                       sthis->pendingConversationsFetchMtx_);
                                   auto& pending = sthis->pendingConversationsFetch_[conversationId];
                                   if (!pending.ready) {
                                       pending.removeId = oldConvId;
                                       if (channel) {
                                           pending.ready = true;
                                           pending.deviceId = channel->deviceId().toString();
                                           pending.socket = channel;
                                           lk.unlock();
                                           sthis->checkConversationsEvents();
                                           return true;
                                       } else {
                                           sthis->stopFetch(conversationId, deviceId);
                                       }
                                   }
                                   return false;
                               },
                               "application/im-gitmessage-id");
                       });
    ConvInfo info;
    info.id = conversationId;
    info.created = std::time(nullptr);
    info.members.emplace_back(pimpl_->username_);
    info.members.emplace_back(uri);
    addConvInfo(info);
}

// Message send/load
void
ConversationModule::sendMessage(const std::string& conversationId,
                                std::string message,
                                const std::string& replyTo,
                                const std::string& type,
                                bool announce,
                                OnCommitCb&& onCommit,
                                OnDoneCb&& cb)
{
    pimpl_->sendMessage(conversationId,
                        std::move(message),
                        replyTo,
                        type,
                        announce,
                        std::move(onCommit),
                        std::move(cb));
}

void
ConversationModule::sendMessage(const std::string& conversationId,
                                Json::Value&& value,
                                const std::string& replyTo,
                                bool announce,
                                OnCommitCb&& onCommit,
                                OnDoneCb&& cb)
{
    pimpl_->sendMessage(conversationId,
                        std::move(value),
                        replyTo,
                        announce,
                        std::move(onCommit),
                        std::move(cb));
}

void
ConversationModule::editMessage(const std::string& conversationId,
                                const std::string& newBody,
                                const std::string& editedId)
{
    pimpl_->editMessage(conversationId, newBody, editedId);
}

void
ConversationModule::reactToMessage(const std::string& conversationId,
                                   const std::string& newBody,
                                   const std::string& reactToId)
{
    // Commit message edition
    Json::Value json;
    json["body"] = newBody;
    json["react-to"] = reactToId;
    json["type"] = "text/plain";
    pimpl_->sendMessage(conversationId, std::move(json));
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
        sendMessage(convId, std::move(value));
    }
}

bool
ConversationModule::onMessageDisplayed(const std::string& peer,
                                       const std::string& conversationId,
                                       const std::string& interactionId)
{
    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    auto conversation = pimpl_->conversations_.find(conversationId);
    if (conversation != pimpl_->conversations_.end() && conversation->second) {
        if (conversation->second->setMessageDisplayed(peer, interactionId)) {
            auto msg = std::make_shared<SyncMsg>();
            std::map<std::string, std::map<std::string, std::string>> ld;
            ld[conversationId] = conversation->second->displayed();
            msg->ld = std::move(ld);
            lk.unlock();
            pimpl_->needsSyncingCb_(std::move(msg));
            return true;
        }
    }
    return false;
}

std::map<std::string, std::map<std::string, std::string>>
ConversationModule::convDisplayed() const
{
    std::map<std::string, std::map<std::string, std::string>> displayed;
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    for (const auto& [id, conv] : pimpl_->conversations_) {
        if (conv) {
            auto d = conv->displayed();
            if (!d.empty())
                displayed[id] = std::move(d);
        }
    }
    return displayed;
}

uint32_t
ConversationModule::loadConversationMessages(const std::string& conversationId,
                                             const std::string& fromMessage,
                                             size_t n)
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto acc = pimpl_->account_.lock();
    auto conversation = pimpl_->conversations_.find(conversationId);
    if (acc && conversation != pimpl_->conversations_.end() && conversation->second) {
        const uint32_t id = std::uniform_int_distribution<uint32_t> {}(acc->rand);
        LogOptions options;
        options.from = fromMessage;
        options.nbOfCommits = n;
        conversation->second->loadMessages(
            [accountId = pimpl_->accountId_, conversationId, id](auto&& messages) {
                emitSignal<libjami::ConversationSignal::ConversationLoaded>(id,
                                                                            accountId,
                                                                            conversationId,
                                                                            messages);
            },
            options);
        return id;
    }
    return 0;
}

uint32_t
ConversationModule::loadConversationUntil(const std::string& conversationId,
                                          const std::string& fromMessage,
                                          const std::string& toMessage)
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto acc = pimpl_->account_.lock();
    auto conversation = pimpl_->conversations_.find(conversationId);
    if (acc && conversation != pimpl_->conversations_.end() && conversation->second) {
        const uint32_t id = std::uniform_int_distribution<uint32_t> {}(acc->rand);
        LogOptions options;
        options.from = fromMessage;
        options.to = toMessage;
        options.includeTo = true;
        conversation->second->loadMessages(
            [accountId = pimpl_->accountId_, conversationId, id](auto&& messages) {
                emitSignal<libjami::ConversationSignal::ConversationLoaded>(id,
                                                                            accountId,
                                                                            conversationId,
                                                                            messages);
            },
            options);
        return id;
    }
    return 0;
}

std::shared_ptr<TransferManager>
ConversationModule::dataTransfer(const std::string& id) const
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto conversation = pimpl_->conversations_.find(id);
    if (conversation != pimpl_->conversations_.end() && conversation->second)
        return conversation->second->dataTransfer();
    return {};
}

bool
ConversationModule::onFileChannelRequest(const std::string& conversationId,
                                         const std::string& member,
                                         const std::string& fileId,
                                         bool verifyShaSum) const
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto conversation = pimpl_->conversations_.find(conversationId);
    if (conversation != pimpl_->conversations_.end() && conversation->second)
        return conversation->second->onFileChannelRequest(member, fileId, verifyShaSum);
    return false;
}

bool
ConversationModule::downloadFile(const std::string& conversationId,
                                 const std::string& interactionId,
                                 const std::string& fileId,
                                 const std::string& path,
                                 size_t start,
                                 size_t end)
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto conversation = pimpl_->conversations_.find(conversationId);
    if (conversation == pimpl_->conversations_.end() || !conversation->second)
        return false;

    return conversation->second->downloadFile(interactionId, fileId, path, "", "", start, end);
}

void
ConversationModule::syncConversations(const std::string& peer, const std::string& deviceId)
{
    // Sync conversations where peer is member
    std::set<std::string> toFetch;
    std::set<std::string> toClone;
    {
        std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
        std::lock_guard<std::mutex> lkCI(pimpl_->convInfosMtx_);
        for (const auto& [key, ci] : pimpl_->convInfos_) {
            auto it = pimpl_->conversations_.find(key);
            if (it != pimpl_->conversations_.end() && it->second) {
                if (!it->second->isRemoving() && it->second->isMember(peer, false))
                    toFetch.emplace(key);
            } else if (!ci.removed
                       && std::find(ci.members.begin(), ci.members.end(), peer)
                              != ci.members.end()) {
                // In this case the conversation was never cloned (can be after an import)
                toClone.emplace(key);
            }
        }
    }
    for (const auto& cid : toFetch)
        pimpl_->fetchNewCommits(peer, deviceId, cid);
    for (const auto& cid : toClone)
        pimpl_->cloneConversation(deviceId, peer, cid);
    if (pimpl_->syncCnt.load() == 0) {
        if (auto acc = pimpl_->account_.lock())
            emitSignal<libjami::ConversationSignal::ConversationSyncFinished>(
                acc->getAccountID().c_str());
    }
}

void
ConversationModule::onSyncData(const SyncMsg& msg,
                               const std::string& peerId,
                               const std::string& deviceId)
{
    for (const auto& [key, convInfo] : msg.c) {
        auto convId = convInfo.id;
        auto removed = convInfo.removed;
        pimpl_->rmConversationRequest(convId);
        if (not removed) {
            // If multi devices, it can detect a conversation that was already
            // removed, so just check if the convinfo contains a removed conv
            auto itConv = pimpl_->convInfos_.find(convId);
            if (itConv != pimpl_->convInfos_.end() && itConv->second.removed) {
                if (itConv->second.removed > convInfo.created) {
                    // Only reclone if re-added, else the peer is not synced yet (could be offline before)
                    continue;
                }
                JAMI_DEBUG("Re-add previously removed conversation {:s}", convId);
            }
            pimpl_->cloneConversation(deviceId, peerId, convId, convInfo.lastDisplayed);
        } else {
            {
                std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
                auto itConv = pimpl_->conversations_.find(convId);
                if (itConv != pimpl_->conversations_.end() && !itConv->second->isRemoving()) {
                    emitSignal<libjami::ConversationSignal::ConversationRemoved>(pimpl_->accountId_,
                                                                                 convId);
                    itConv->second->setRemovingFlag();
                }
            }
            std::unique_lock<std::mutex> lk(pimpl_->convInfosMtx_);
            auto& ci = pimpl_->convInfos_;
            auto itConv = ci.find(convId);
            if (itConv != ci.end()) {
                itConv->second.removed = std::time(nullptr);
                if (convInfo.erased) {
                    itConv->second.erased = std::time(nullptr);
                    pimpl_->saveConvInfos();
                    lk.unlock();
                    pimpl_->removeRepository(convId, false);
                }
            }
        }
    }

    for (const auto& [convId, req] : msg.cr) {
        if (pimpl_->isAcceptedConversation(convId)) {
            // Already accepted request
            pimpl_->rmConversationRequest(convId);
            continue;
        }

        // New request
        if (!pimpl_->addConversationRequest(convId, req))
            continue;

        if (req.declined != 0) {
            // Request declined
            JAMI_LOG("[Account {:s}] Declined request detected for conversation {:s} (device {:s})",
                      pimpl_->accountId_,
                      convId,
                      deviceId);
            emitSignal<libjami::ConversationSignal::ConversationRequestDeclined>(pimpl_->accountId_,
                                                                                 convId);
            continue;
        }

        JAMI_LOG("[Account {:s}] New request detected for conversation {:s} (device {:s})",
                  pimpl_->accountId_,
                  convId,
                  deviceId);

        emitSignal<libjami::ConversationSignal::ConversationRequestReceived>(pimpl_->accountId_,
                                                                             convId,
                                                                             req.toMap());
    }

    // Updates preferences for conversations
    for (const auto& [convId, p] : msg.p) {
        {
            std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
            auto itConv = pimpl_->conversations_.find(convId);
            if (itConv != pimpl_->conversations_.end() && itConv->second) {
                itConv->second->updatePreferences(p);
                continue;
            }
        }
        std::lock_guard<std::mutex> lk(pimpl_->pendingConversationsFetchMtx_);
        auto itFetch = pimpl_->pendingConversationsFetch_.find(convId);
        if (itFetch != pimpl_->pendingConversationsFetch_.end())
            itFetch->second.preferences = p;
    }

    // Updates displayed for conversations
    for (const auto& [convId, ld] : msg.ld) {
        {
            std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
            auto itConv = pimpl_->conversations_.find(convId);
            if (itConv != pimpl_->conversations_.end() && itConv->second) {
                itConv->second->updateLastDisplayed(ld);
                continue;
            }
        }
        std::lock_guard<std::mutex> lk(pimpl_->pendingConversationsFetchMtx_);
        auto itFetch = pimpl_->pendingConversationsFetch_.find(convId);
        if (itFetch != pimpl_->pendingConversationsFetch_.end())
            itFetch->second.lastDisplayed = ld;
    }
}

bool
ConversationModule::needsSyncingWith(const std::string& memberUri, const std::string& deviceId) const
{
    // Check if a conversation needs to fetch remote or to be cloned
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    std::lock_guard<std::mutex> lkCI(pimpl_->convInfosMtx_);
    for (const auto& [key, ci] : pimpl_->convInfos_) {
        auto it = pimpl_->conversations_.find(key);
        if (it != pimpl_->conversations_.end() && it->second) {
            if (!it->second->isRemoving() && it->second->isMember(memberUri, false)
                && it->second->needsFetch(deviceId))
                return true;
        } else if (!ci.removed
                   && std::find(ci.members.begin(), ci.members.end(), memberUri)
                          != ci.members.end()) {
            // In this case the conversation was never cloned (can be after an import)
            return true;
        }
    }
    return false;
}

void
ConversationModule::setFetched(const std::string& conversationId,
                               const std::string& deviceId,
                               const std::string& commitId)
{
    auto remove = false;
    {
        std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
        auto it = pimpl_->conversations_.find(conversationId);
        if (it != pimpl_->conversations_.end() && it->second) {
            remove = it->second->isRemoving();
            it->second->hasFetched(deviceId, commitId);
        }
    }
    if (remove)
        pimpl_->removeRepository(conversationId, true);
}

void
ConversationModule::onNewCommit(const std::string& peer,
                                const std::string& deviceId,
                                const std::string& conversationId,
                                const std::string& commitId)
{
    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    auto itConv = pimpl_->convInfos_.find(conversationId);
    if (itConv != pimpl_->convInfos_.end() && itConv->second.removed) {
        // If the conversation is removed and we receives a new commit,
        // it means that the contact was removed but not banned. So we can generate
        // a new trust request
        JAMI_WARNING("[Account {:s}] Could not find conversation {:s}, ask for an invite",
                  pimpl_->accountId_,
                  conversationId);
        pimpl_->sendMsgCb_(peer,
                           std::map<std::string, std::string> {
                               {"application/invite", conversationId}},
                           0);
        return;
    }
    JAMI_DEBUG("[Account {:s}] on new commit notification from {:s}, for {:s}, commit {:s}",
             pimpl_->accountId_,
             peer,
             conversationId,
             commitId);
    lk.unlock();
    pimpl_->fetchNewCommits(peer, deviceId, conversationId, commitId);
}

void
ConversationModule::addConversationMember(const std::string& conversationId,
                                          const std::string& contactUri,
                                          bool sendRequest)
{
    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    // Add a new member in the conversation
    auto it = pimpl_->conversations_.find(conversationId);
    if (it == pimpl_->conversations_.end()) {
        JAMI_ERROR("Conversation {:s} doesn't exist", conversationId);
        return;
    }

    if (it->second->isMember(contactUri, true)) {
        JAMI_DEBUG("{:s} is already a member of {:s}, resend invite",
                 contactUri,
                 conversationId);
        // Note: This should not be necessary, but if for whatever reason the other side didn't join
        // we should not forbid new invites
        auto invite = it->second->generateInvitation();
        lk.unlock();
        pimpl_->sendMsgCb_(contactUri, std::move(invite), 0);
        return;
    }

    it->second
        ->addMember(contactUri,
                    [this, conversationId, sendRequest, contactUri](bool ok,
                                                                    const std::string& commitId) {
                        if (ok) {
                            std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
                            auto it = pimpl_->conversations_.find(conversationId);
                            if (it != pimpl_->conversations_.end() && it->second) {
                                pimpl_->sendMessageNotification(*it->second,
                                                                commitId,
                                                                true); // For the other members
                                if (sendRequest) {
                                    auto invite = it->second->generateInvitation();
                                    lk.unlock();
                                    pimpl_->sendMsgCb_(contactUri, std::move(invite), 0);
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
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto it = pimpl_->conversations_.find(conversationId);
    if (it != pimpl_->conversations_.end() && it->second) {
        it->second->removeMember(contactUri,
                                 isDevice,
                                 [this, conversationId](bool ok, const std::string& commitId) {
                                     if (ok) {
                                         pimpl_->sendMessageNotification(conversationId,
                                                                         commitId,
                                                                         true);
                                     }
                                 });
    }
}

std::vector<std::map<std::string, std::string>>
ConversationModule::getConversationMembers(const std::string& conversationId, bool includeBanned) const
{
    return pimpl_->getConversationMembers(conversationId, includeBanned);
}

uint32_t
ConversationModule::countInteractions(const std::string& convId,
                                      const std::string& toId,
                                      const std::string& fromId,
                                      const std::string& authorUri) const
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto conversation = pimpl_->conversations_.find(convId);
    if (conversation != pimpl_->conversations_.end() && conversation->second) {
        return conversation->second->countInteractions(toId, fromId, authorUri);
    }
    return 0;
}

void
ConversationModule::search(uint32_t req, const std::string& convId, const Filter& filter) const
{
    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    auto finishedFlag = std::make_shared<std::atomic_int>(pimpl_->conversations_.size());
    for (const auto& [cid, conversation] : pimpl_->conversations_) {
        if (!conversation || (!convId.empty() && convId != cid)) {
            if ((*finishedFlag)-- == 1) {
                emitSignal<libjami::ConversationSignal::MessagesFound>(
                    req,
                    pimpl_->accountId_,
                    std::string {},
                    std::vector<std::map<std::string, std::string>> {});
            }
            continue;
        }
        conversation->search(req, filter, finishedFlag);
    }
}

void
ConversationModule::updateConversationInfos(const std::string& conversationId,
                                            const std::map<std::string, std::string>& infos,
                                            bool sync)
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto it = pimpl_->conversations_.find(conversationId);
    if (it == pimpl_->conversations_.end()) {
        JAMI_ERROR("Conversation {:s} doesn't exist", conversationId);
        return;
    }

    it->second->updateInfos(infos,
                            [this, conversationId, sync](bool ok, const std::string& commitId) {
                                if (ok && sync) {
                                    pimpl_->sendMessageNotification(conversationId, commitId, true);
                                } else if (sync)
                                    JAMI_WARNING("Couldn't update infos on {:s}", conversationId);
                            });
}

std::map<std::string, std::string>
ConversationModule::conversationInfos(const std::string& conversationId) const
{
    {
        std::lock_guard<std::mutex> lk(pimpl_->conversationsRequestsMtx_);
        auto itReq = pimpl_->conversationsRequests_.find(conversationId);
        if (itReq != pimpl_->conversationsRequests_.end())
            return itReq->second.metadatas;
    }
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto it = pimpl_->conversations_.find(conversationId);
    if (it == pimpl_->conversations_.end() or not it->second) {
        std::lock_guard<std::mutex> lkCi(pimpl_->convInfosMtx_);
        auto itConv = pimpl_->convInfos_.find(conversationId);
        if (itConv == pimpl_->convInfos_.end()) {
            JAMI_ERROR("Conversation {:s} doesn't exist", conversationId);
            return {};
        }
        return {{"syncing", "true"}};
    }

    return it->second->infos();
}

void
ConversationModule::setConversationPreferences(const std::string& conversationId,
                                               const std::map<std::string, std::string>& prefs)
{
    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    auto it = pimpl_->conversations_.find(conversationId);
    if (it == pimpl_->conversations_.end()) {
        JAMI_ERROR("Conversation {:s} doesn't exist", conversationId);
        return;
    }

    it->second->updatePreferences(prefs);
    auto msg = std::make_shared<SyncMsg>();
    std::map<std::string, std::map<std::string, std::string>> p;
    p[conversationId] = it->second->preferences(true);
    msg->p = std::move(p);
    lk.unlock();
    pimpl_->needsSyncingCb_(std::move(msg));
}

std::map<std::string, std::string>
ConversationModule::getConversationPreferences(const std::string& conversationId, bool includeCreated) const
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto it = pimpl_->conversations_.find(conversationId);
    if (it == pimpl_->conversations_.end() or not it->second)
        return {};

    return it->second->preferences(includeCreated);
}

std::map<std::string, std::map<std::string, std::string>>
ConversationModule::convPreferences() const
{
    std::map<std::string, std::map<std::string, std::string>> p;
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    for (const auto& [id, conv] : pimpl_->conversations_) {
        if (conv) {
            auto prefs = conv->preferences(true);
            if (!prefs.empty())
                p[id] = std::move(prefs);
        }
    }
    return p;
}

std::vector<uint8_t>
ConversationModule::conversationVCard(const std::string& conversationId) const
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    // Add a new member in the conversation
    auto it = pimpl_->conversations_.find(conversationId);
    if (it == pimpl_->conversations_.end() || !it->second) {
        JAMI_ERROR("Conversation {:s} doesn't exist", conversationId);
        return {};
    }

    return it->second->vCard();
}

bool
ConversationModule::isBannedDevice(const std::string& convId, const std::string& deviceId) const
{
    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    auto conversation = pimpl_->conversations_.find(convId);
    return conversation == pimpl_->conversations_.end() || !conversation->second
           || conversation->second->isBanned(deviceId);
}

void
ConversationModule::removeContact(const std::string& uri, bool banned)
{
    // Remove linked conversation's requests
    {
        std::lock_guard<std::mutex> lk(pimpl_->conversationsRequestsMtx_);
        auto update = false;
        auto it = pimpl_->conversationsRequests_.begin();
        while (it != pimpl_->conversationsRequests_.end()) {
            if (it->second.from == uri) {
                emitSignal<libjami::ConversationSignal::ConversationRequestDeclined>(
                    pimpl_->accountId_, it->first);
                update = true;
                it = pimpl_->conversationsRequests_.erase(it);
            } else {
                ++it;
            }
        }
        if (update)
            pimpl_->saveConvRequests();
    }
    if (banned)
        return; // Keep the conversation in banned model
    // Remove related conversation
    auto isSelf = uri == pimpl_->username_;
    std::vector<std::string> toRm;
    std::vector<std::string> updated;
    auto updateClient = [&](const auto& convId) {
        if (pimpl_->updateConvReqCb_)
            pimpl_->updateConvReqCb_(convId, uri, false);
        emitSignal<libjami::ConversationSignal::ConversationRemoved>(pimpl_->accountId_, convId);
    };
    {
        std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
        std::lock_guard<std::mutex> lkCi(pimpl_->convInfosMtx_);
        for (auto& [convId, conv] : pimpl_->convInfos_) {
            auto itConv = pimpl_->conversations_.find(convId);
            if (itConv != pimpl_->conversations_.end() && itConv->second) {
                try {
                    // Note it's important to check getUsername(), else
                    // removing self can remove all conversations
                    if (itConv->second->mode() == ConversationMode::ONE_TO_ONE) {
                        auto initMembers = itConv->second->getInitialMembers();
                        if ((isSelf && initMembers.size() == 1)
                            || (!isSelf
                                && std::find(initMembers.begin(), initMembers.end(), uri)
                                       != initMembers.end())) {
                            // Mark as removed
                            conv.removed = std::time(nullptr);
                            toRm.emplace_back(convId);
                            updateClient(convId);
                        }
                    }
                } catch (const std::exception& e) {
                    JAMI_WARN("%s", e.what());
                }
            } else if (std::find(conv.members.begin(), conv.members.end(), uri)
                       != conv.members.end()) {
                // It's syncing with uri, mark as removed!
                conv.removed = std::time(nullptr);
                updated.emplace_back(convId);
                updateClient(convId);
            }
        }
        if (!updated.empty() || !toRm.empty())
            pimpl_->saveConvInfos();
    }
    // Note, if we ban the device, we don't send the leave cause the other peer will just
    // never got the notifications, so just erase the datas
    for (const auto& id : toRm)
        pimpl_->removeRepository(id, true, true);
}

bool
ConversationModule::removeConversation(const std::string& conversationId)
{
    return pimpl_->removeConversation(conversationId);
}

void
ConversationModule::initReplay(const std::string& oldConvId, const std::string& newConvId)
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto acc = pimpl_->account_.lock();
    auto conversation = pimpl_->conversations_.find(oldConvId);
    if (acc && conversation != pimpl_->conversations_.end() && conversation->second) {
        std::promise<bool> waitLoad;
        std::future<bool> fut = waitLoad.get_future();
        // we should wait for loadMessage, because it will be deleted after this.
        conversation->second->loadMessages(
            [&](auto&& messages) {
                std::reverse(messages.begin(),
                             messages.end()); // Log is inverted as we want to replay
                std::lock_guard<std::mutex> lk(pimpl_->replayMtx_);
                pimpl_->replay_[newConvId] = std::move(messages);
                waitLoad.set_value(true);
            },
            {});
        fut.wait();
    }
}

bool
ConversationModule::isHosting(const std::string& conversationId, const std::string& confId) const
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    if (conversationId.empty()) {
        return std::find_if(pimpl_->conversations_.cbegin(),
                            pimpl_->conversations_.cend(),
                            [&](const auto& conv) { return conv.second->isHosting(confId); })
               != pimpl_->conversations_.cend();
    } else {
        auto conversation = pimpl_->conversations_.find(conversationId);
        if (conversation != pimpl_->conversations_.end() && conversation->second) {
            return conversation->second->isHosting(confId);
        }
    }
    return false;
}

std::vector<std::map<std::string, std::string>>
ConversationModule::getActiveCalls(const std::string& conversationId) const
{
    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    auto conversation = pimpl_->conversations_.find(conversationId);
    if (conversation == pimpl_->conversations_.end() || !conversation->second) {
        JAMI_ERR("Conversation %s not found", conversationId.c_str());
        return {};
    }
    return conversation->second->currentCalls();
}

void
ConversationModule::call(const std::string& url,
                         const std::shared_ptr<SIPCall>& call,
                         std::function<void(const std::string&, const DeviceId&)>&& cb)
{
    std::string conversationId = "", confId = "", uri = "", deviceId = "";
    if (url.find('/') == std::string::npos) {
        conversationId = url;
    } else {
        auto parameters = jami::split_string(url, '/');
        if (parameters.size() != 4) {
            JAMI_ERROR("Incorrect url {:s}", url);
            return;
        }
        conversationId = parameters[0];
        uri = parameters[1];
        deviceId = parameters[2];
        confId = parameters[3];
    }

    std::string callUri;
    auto sendCall = [&]() {
        call->setState(Call::ConnectionState::TRYING);
        call->setPeerNumber(callUri);
        call->setPeerUri("rdv:" + callUri);
        call->addStateListener([w = pimpl_->weak(), conversationId](Call::CallState call_state,
                                                                    Call::ConnectionState cnx_state,
                                                                    int) {
            if (cnx_state == Call::ConnectionState::DISCONNECTED
                && call_state == Call::CallState::MERROR) {
                auto shared = w.lock();
                if (!shared)
                    return false;
                if (auto acc = shared->account_.lock())
                    emitSignal<libjami::ConfigurationSignal::NeedsHost>(acc->getAccountID(),
                                                                        conversationId);
                return true;
            }
            return true;
        });
        cb(callUri, DeviceId(deviceId));
    };

    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    auto conversation = pimpl_->conversations_.find(conversationId);
    if (conversation == pimpl_->conversations_.end() || !conversation->second) {
        JAMI_ERROR("Conversation {:s} not found", conversationId);
        return;
    }

    // Check if we want to join a specific conference
    // So, if confId is specified or if there is some activeCalls
    // or if we are the default host.
    auto& conv = conversation->second;
    auto activeCalls = conv->currentCalls();
    auto infos = conv->infos();
    auto itRdvAccount = infos.find("rdvAccount");
    auto itRdvDevice = infos.find("rdvDevice");
    auto sendCallRequest = false;
    if (confId != "") {
        sendCallRequest = true;
        confId = confId == "0" ? Manager::instance().callFactory.getNewCallID() : confId;
        JAMI_DEBUG("Calling self, join conference");
    } else if (!activeCalls.empty()) {
        // Else, we try to join active calls
        sendCallRequest = true;
        auto& ac = *activeCalls.rbegin();
        confId = ac.at("id");
        uri = ac.at("uri");
        deviceId = ac.at("device");
        JAMI_DEBUG("Calling last active call: {:s}", callUri);
    } else if (itRdvAccount != infos.end() && itRdvDevice != infos.end()) {
        // Else, creates "to" (accountId/deviceId/conversationId/confId) and ask remote host
        sendCallRequest = true;
        uri = itRdvAccount->second;
        deviceId = itRdvDevice->second;
        confId = call->getCallId();
        JAMI_DEBUG("Remote host detected. Calling {:s} on device {:s}", uri, deviceId);
    }

    if (sendCallRequest) {
        callUri = fmt::format("{}/{}/{}/{}", conversationId, uri, deviceId, confId);
        if (uri == pimpl_->username_ && deviceId == pimpl_->deviceId_) {
            // In this case, we're probably hosting the conference.
            call->setState(Call::ConnectionState::CONNECTED);
            // In this case, the call is the only one in the conference
            // and there is no peer, so media succeeded and are shown to
            // the client.
            call->reportMediaNegotiationStatus();
            lk.unlock();
            hostConference(conversationId, confId, call->getCallId());
            return;
        }
        JAMI_DEBUG("Calling: {:s}", callUri);
        sendCall();
        return;
    }

    // Else, we are the host.
    confId = Manager::instance().callFactory.getNewCallID();
    call->setState(Call::ConnectionState::CONNECTED);
    // In this case, the call is the only one in the conference
    // and there is no peer, so media succeeded and are shown to
    // the client.
    call->reportMediaNegotiationStatus();
    lk.unlock();
    hostConference(conversationId, confId, call->getCallId());
}

void
ConversationModule::hostConference(const std::string& conversationId,
                                   const std::string& confId,
                                   const std::string& callId)
{
    auto acc = pimpl_->account_.lock();
    if (!acc)
        return;
    std::shared_ptr<Call> call;
    call = acc->getCall(callId);
    if (!call) {
        JAMI_WARN("No call with id %s found", callId.c_str());
        return;
    }
    auto conf = acc->getConference(confId);
    auto createConf = !conf;
    if (createConf) {
        conf = std::make_shared<Conference>(acc, confId, true, call->getMediaAttributeList());
        acc->attach(conf);
    }
    conf->addParticipant(callId);

    if (createConf) {
        emitSignal<libjami::CallSignal::ConferenceCreated>(acc->getAccountID(), confId);
    } else {
        conf->attachLocalParticipant();
        conf->reportMediaNegotiationStatus();
        emitSignal<libjami::CallSignal::ConferenceChanged>(acc->getAccountID(),
                                                           conf->getConfId(),
                                                           conf->getStateStr());
        return;
    }

    std::unique_lock<std::mutex> lk(pimpl_->conversationsMtx_);
    auto conversation = pimpl_->conversations_.find(conversationId);
    if (conversation == pimpl_->conversations_.end() || !conversation->second) {
        JAMI_ERR("Conversation %s not found", conversationId.c_str());
        return;
    }
    auto& conv = conversation->second;
    // Add commit to conversation
    Json::Value value;
    value["uri"] = pimpl_->username_;
    value["device"] = pimpl_->deviceId_;
    value["confId"] = confId;
    value["type"] = "application/call-history+json";
    conv->hostConference(std::move(value),
                         [w = pimpl_->weak(),
                                    conversationId](bool ok, const std::string& commitId) {
                             if (ok) {
                                 if (auto shared = w.lock())
                                     shared->sendMessageNotification(conversationId, commitId, true);
                             } else {
                                 JAMI_ERR("Failed to send message to conversation %s",
                                          conversationId.c_str());
                             }
                         });

    // When conf finished = remove host & commit
    // Master call, so when it's stopped, the conference will be stopped (as we use the hold state
    // for detaching the call)
    conf->onShutdown(
        [w = pimpl_->weak(), accountUri = pimpl_->username_, confId, conversationId, call](
            int duration) {
            auto shared = w.lock();
            if (shared) {
                Json::Value value;
                value["uri"] = accountUri;
                value["device"] = shared->deviceId_;
                value["confId"] = confId;
                value["type"] = "application/call-history+json";
                value["duration"] = std::to_string(duration);

                std::unique_lock<std::mutex> lk(shared->conversationsMtx_);
                auto conversation = shared->conversations_.find(conversationId);
                if (conversation == shared->conversations_.end() || !conversation->second) {
                    JAMI_ERR("Conversation %s not found", conversationId.c_str());
                    return true;
                }
                auto& conv = conversation->second;
                conv->removeActiveConference(
                    std::move(value), [w, conversationId](bool ok, const std::string& commitId) {
                        if (ok) {
                            if (auto shared = w.lock()) {
                                shared->sendMessageNotification(conversationId, commitId, true);
                            }
                        } else {
                            JAMI_ERR("Failed to send message to conversation %s",
                                     conversationId.c_str());
                        }
                    });
            }
            return true;
        });
}

std::map<std::string, ConvInfo>
ConversationModule::convInfos(const std::string& accountId)
{
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId;
    return convInfosFromPath(path);
}

std::map<std::string, ConvInfo>
ConversationModule::convInfosFromPath(const std::string& path)
{
    std::map<std::string, ConvInfo> convInfos;
    try {
        // read file
        std::lock_guard<std::mutex> lock(
            fileutils::getFileLock(path + DIR_SEPARATOR_STR + "convInfo"));
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
    auto path = fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId;
    return convRequestsFromPath(path);
}

std::map<std::string, ConversationRequest>
ConversationModule::convRequestsFromPath(const std::string& path)
{
    std::map<std::string, ConversationRequest> convRequests;
    try {
        // read file
        std::lock_guard<std::mutex> lock(
            fileutils::getFileLock(path + DIR_SEPARATOR_STR + "convRequests"));
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
    pimpl_->addConvInfo(info);
}

void
ConversationModule::setConversationMembers(const std::string& convId,
                                           const std::vector<std::string>& members)
{
    std::lock_guard<std::mutex> lk(pimpl_->convInfosMtx_);
    auto convIt = pimpl_->convInfos_.find(convId);
    if (convIt != pimpl_->convInfos_.end()) {
        convIt->second.members = members;
        pimpl_->saveConvInfos();
    }
}

std::shared_ptr<Conversation>
ConversationModule::getConversation(const std::string& convId)
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto convIt = pimpl_->conversations_.find(convId);
    if (convIt != pimpl_->conversations_.end())
        return convIt->second;
    return nullptr;
}

std::shared_ptr<ChannelSocket>
ConversationModule::gitSocket(std::string_view deviceId, std::string_view convId) const
{
    {
        std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
        auto convIt = pimpl_->conversations_.find(convId);
        if (convIt != pimpl_->conversations_.end())
            return convIt->second->gitSocket(DeviceId(deviceId));
    }
    std::lock_guard<std::mutex> lk(pimpl_->pendingConversationsFetchMtx_);
    auto it = pimpl_->pendingConversationsFetch_.find(convId);
    if (it != pimpl_->pendingConversationsFetch_.end())
        return it->second.socket;
    return nullptr;
}
void
ConversationModule::removeGitSocket(std::string_view deviceId, std::string_view convId)
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto convIt = pimpl_->conversations_.find(convId);
    if (convIt != pimpl_->conversations_.end())
        return convIt->second->removeGitSocket(DeviceId(deviceId));
}
void
ConversationModule::shutdownConnections()
{
    {
        std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
        for (auto& [k, conversation] : pimpl_->conversations_)
            conversation->removeGitSockets();
    }
    {
        std::lock_guard<std::mutex> lk(pimpl_->pendingConversationsFetchMtx_);
        for (auto& [k, pending] : pimpl_->pendingConversationsFetch_)
            pending.socket = {};
    }
}
void
ConversationModule::addSwarmChannel(const std::string& conversationId,
                                    std::shared_ptr<ChannelSocket> channel)
{
    std::lock_guard<std::mutex> lk(pimpl_->conversationsMtx_);
    auto convIt = pimpl_->conversations_.find(conversationId);
    if (convIt != pimpl_->conversations_.end())
        convIt->second->addSwarmChannel(channel);
}

} // namespace jami
