/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "conversation_module.h"

#include <algorithm>
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
#include "json_utils.h"

namespace jami {

using ConvInfoMap = std::map<std::string, ConvInfo>;

struct PendingConversationFetch
{
    bool ready {false};
    bool cloning {false};
    std::string deviceId {};
    std::string removeId {};
    std::map<std::string, std::string> preferences {};
    std::map<std::string, std::map<std::string, std::string>> status {};
    std::set<std::string> connectingTo {};
    std::shared_ptr<dhtnet::ChannelSocket> socket {};
};

constexpr std::chrono::seconds MAX_FALLBACK {12 * 3600s};

struct SyncedConversation
{
    std::mutex mtx;
    std::unique_ptr<asio::steady_timer> fallbackClone;
    std::chrono::seconds fallbackTimer {5s};
    ConvInfo info;
    std::unique_ptr<PendingConversationFetch> pending;
    std::shared_ptr<Conversation> conversation;

    SyncedConversation(const std::string& convId)
        : info {convId}
    {
        fallbackClone = std::make_unique<asio::steady_timer>(*Manager::instance().ioContext());
    }
    SyncedConversation(const ConvInfo& info)
        : info {info}
    {
        fallbackClone = std::make_unique<asio::steady_timer>(*Manager::instance().ioContext());
    }

    bool startFetch(const std::string& deviceId, bool checkIfConv = false)
    {
        // conversation mtx must be locked
        if (checkIfConv && conversation)
            return false; // Already a conversation
        if (pending) {
            if (pending->ready)
                return false; // Already doing stuff
            // if (pending->deviceId == deviceId)
            //     return false; // Already fetching
            if (pending->connectingTo.find(deviceId) != pending->connectingTo.end())
                return false; // Already connecting to this device
        } else {
            pending = std::make_unique<PendingConversationFetch>();
            pending->connectingTo.insert(deviceId);
            return true;
        }
        return true;
    }

    void stopFetch(const std::string& deviceId)
    {
        // conversation mtx must be locked
        if (!pending)
            return;
        pending->connectingTo.erase(deviceId);
        if (pending->connectingTo.empty())
            pending.reset();
    }

    std::vector<std::map<std::string, std::string>> getMembers(bool includeLeft,
                                                               bool includeBanned) const
    {
        // conversation mtx must be locked
        if (conversation)
            return conversation->getMembers(true, includeLeft, includeBanned);
        // If we're cloning, we can return the initial members
        std::vector<std::map<std::string, std::string>> result;
        result.reserve(info.members.size());
        for (const auto& uri : info.members) {
            result.emplace_back(std::map<std::string, std::string> {{"uri", uri}});
        }
        return result;
    }
};

class ConversationModule::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::shared_ptr<JamiAccount>&& account,
         std::shared_ptr<AccountManager>&& accountManager,
         NeedsSyncingCb&& needsSyncingCb,
         SengMsgCb&& sendMsgCb,
         NeedSocketCb&& onNeedSocket,
         NeedSocketCb&& onNeedSwarmSocket,
         OneToOneRecvCb&& oneToOneRecvCb);

    template<typename S, typename T>
    inline auto withConv(const S& convId, T&& cb) const
    {
        if (auto conv = getConversation(convId)) {
            std::lock_guard lk(conv->mtx);
            return cb(*conv);
        } else {
            JAMI_WARNING("Conversation {} not found", convId);
        }
        return decltype(cb(std::declval<SyncedConversation&>()))();
    }
    template<typename S, typename T>
    inline auto withConversation(const S& convId, T&& cb)
    {
        if (auto conv = getConversation(convId)) {
            std::lock_guard lk(conv->mtx);
            if (conv->conversation)
                return cb(*conv->conversation);
        } else {
            JAMI_WARNING("Conversation {} not found", convId);
        }
        return decltype(cb(std::declval<Conversation&>()))();
    }

    // Retrieving recent commits
    /**
     * Clone a conversation (initial) from device
     * @param deviceId
     * @param convId
     */
    void cloneConversation(const std::string& deviceId,
                           const std::string& peer,
                           const std::string& convId);
    void cloneConversation(const std::string& deviceId,
                           const std::string& peer,
                           const std::shared_ptr<SyncedConversation>& conv);

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
    void setConversationMembers(const std::string& convId, const std::set<std::string>& members);

    /**
     * Remove a repository and all files
     * @param convId
     * @param sync      If we send an update to other account's devices
     * @param force     True if ignore the removing flag
     */
    void removeRepository(const std::string& convId, bool sync, bool force = false);
    void removeRepositoryImpl(SyncedConversation& conv, bool sync, bool force = false);
    /**
     * Remove a conversation
     * @param conversationId
     */
    bool removeConversation(const std::string& conversationId);
    bool removeConversationImpl(SyncedConversation& conv);

    /**
     * Send a message notification to all members
     * @param conversation
     * @param commit
     * @param sync      If we send an update to other account's devices
     * @param deviceId  If we need to filter a specific device
     */
    void sendMessageNotification(const std::string& conversationId,
                                 bool sync,
                                 const std::string& commitId = "",
                                 const std::string& deviceId = "");
    void sendMessageNotification(Conversation& conversation,
                                 bool sync,
                                 const std::string& commitId = "",
                                 const std::string& deviceId = "");

    /**
     * @return if a convId is a valid conversation (repository cloned & usable)
     */
    bool isConversation(const std::string& convId) const
    {
        std::lock_guard lk(conversationsMtx_);
        auto c = conversations_.find(convId);
        return c != conversations_.end() && c->second;
    }

    void addConvInfo(const ConvInfo& info)
    {
        std::lock_guard lk(convInfosMtx_);
        convInfos_[info.id] = info;
        saveConvInfos();
    }

    std::string getOneToOneConversation(const std::string& uri) const noexcept;

    bool updateConvForContact(const std::string& uri,
                              const std::string& oldConv,
                              const std::string& newConv);

    std::shared_ptr<SyncedConversation> getConversation(std::string_view convId) const
    {
        std::lock_guard lk(conversationsMtx_);
        auto c = conversations_.find(convId);
        return c != conversations_.end() ? c->second : nullptr;
    }
    std::shared_ptr<SyncedConversation> getConversation(std::string_view convId)
    {
        std::lock_guard lk(conversationsMtx_);
        auto c = conversations_.find(convId);
        return c != conversations_.end() ? c->second : nullptr;
    }
    std::shared_ptr<SyncedConversation> startConversation(const std::string& convId)
    {
        std::lock_guard lk(conversationsMtx_);
        auto& c = conversations_[convId];
        if (!c)
            c = std::make_shared<SyncedConversation>(convId);
        return c;
    }
    std::shared_ptr<SyncedConversation> startConversation(const ConvInfo& info)
    {
        std::lock_guard lk(conversationsMtx_);
        auto& c = conversations_[info.id];
        if (!c)
            c = std::make_shared<SyncedConversation>(info);
        return c;
    }
    std::vector<std::shared_ptr<SyncedConversation>> getSyncedConversations() const
    {
        std::lock_guard lk(conversationsMtx_);
        std::vector<std::shared_ptr<SyncedConversation>> result;
        result.reserve(conversations_.size());
        for (const auto& [_, c] : conversations_)
            result.emplace_back(c);
        return result;
    }
    std::vector<std::shared_ptr<Conversation>> getConversations() const
    {
        std::lock_guard lk(conversationsMtx_);
        std::vector<std::shared_ptr<Conversation>> result;
        result.reserve(conversations_.size());
        for (const auto& [_, sc] : conversations_) {
            if (auto c = sc->conversation)
                result.emplace_back(std::move(c));
        }
        return result;
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

    void bootstrapCb(std::string convId);

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
    void declineOtherConversationWith(const std::string& uri) noexcept;
    bool addConversationRequest(const std::string& id, const ConversationRequest& req)
    {
        // conversationsRequestsMtx_ MUST BE LOCKED
        if (isConversation(id))
            return false;
        auto it = conversationsRequests_.find(id);
        if (it != conversationsRequests_.end()) {
            // We only remove requests (if accepted) or change .declined
            if (!req.declined)
                return false;
        } else if (req.isOneToOne()) {
            // Check that we're not adding a second one to one trust request
            // NOTE: If a new one to one request is received, we can decline the previous one.
            declineOtherConversationWith(req.from);
        }
        JAMI_DEBUG("[Account {}] [Conversation {}] Adding conversation request from {}", accountId_, id, req.from);
        conversationsRequests_[id] = req;
        saveConvRequests();
        return true;
    }
    void rmConversationRequest(const std::string& id)
    {
        // conversationsRequestsMtx_ MUST BE LOCKED
        auto it = conversationsRequests_.find(id);
        if (it != conversationsRequests_.end()) {
            auto& md = syncingMetadatas_[id];
            md = it->second.metadatas;
            md["syncing"] = "true";
            md["created"] = std::to_string(it->second.received);
        }
        saveMetadata();
        conversationsRequests_.erase(id);
        saveConvRequests();
    }

    std::weak_ptr<JamiAccount> account_;
    std::shared_ptr<AccountManager> accountManager_;
    const std::string accountId_ {};
    NeedsSyncingCb needsSyncingCb_;
    SengMsgCb sendMsgCb_;
    NeedSocketCb onNeedSocket_;
    NeedSocketCb onNeedSwarmSocket_;
    OneToOneRecvCb oneToOneRecvCb_;

    std::string deviceId_ {};
    std::string username_ {};

    // Requests
    mutable std::mutex conversationsRequestsMtx_;
    std::map<std::string, ConversationRequest> conversationsRequests_;

    // Conversations
    mutable std::mutex conversationsMtx_ {};
    std::map<std::string, std::shared_ptr<SyncedConversation>, std::less<>> conversations_;

    // The following information are stored on the disk
    mutable std::mutex convInfosMtx_; // Note, should be locked after conversationsMtx_ if needed
    std::map<std::string, ConvInfo> convInfos_;

    // When sending a new message, we need to send the notification to some peers of the
    // conversation However, the conversation may be not bootstraped, so the list will be empty.
    // notSyncedNotification_ will store the notifiaction to announce until we have peers to sync
    // with.
    std::mutex notSyncedNotificationMtx_;
    std::map<std::string, std::string> notSyncedNotification_;

    std::weak_ptr<Impl> weak() { return std::static_pointer_cast<Impl>(shared_from_this()); }

    // Replay conversations (after erasing/re-adding)
    std::mutex replayMtx_;
    std::map<std::string, std::vector<std::map<std::string, std::string>>> replay_;
    std::map<std::string, uint64_t> refreshMessage;
    std::atomic_int syncCnt {0};

#ifdef LIBJAMI_TEST
    std::function<void(std::string, Conversation::BootstrapStatus)> bootstrapCbTest_;
#endif

    void fixStructures(
        std::shared_ptr<JamiAccount> account,
        const std::vector<std::tuple<std::string, std::string, std::string>>& updateContactConv,
        const std::set<std::string>& toRm);

    void cloneConversationFrom(const std::shared_ptr<SyncedConversation> conv,
                               const std::string& deviceId,
                               const std::string& oldConvId = "");
    void bootstrap(const std::string& convId);
    void fallbackClone(const asio::error_code& ec, const std::string& conversationId);
    void cloneConversationFrom(const std::string& conversationId,
                               const std::string& uri,
                               const std::string& oldConvId = "");

    // While syncing, we do not want to lose metadata (avatar/title and mode)
    std::map<std::string, std::map<std::string, std::string>> syncingMetadatas_;
    void saveMetadata()
    {
        auto path = fileutils::get_data_dir() / accountId_;
        std::ofstream file(path / "syncingMetadatas", std::ios::trunc | std::ios::binary);
        msgpack::pack(file, syncingMetadatas_);
    }

    void loadMetadata()
    {
        try {
            // read file
            auto path = fileutils::get_data_dir() / accountId_;
            std::lock_guard lock(dhtnet::fileutils::getFileLock(path / "syncingMetadatas"));
            auto file = fileutils::loadFile("syncingMetadatas", path);
            // load values
            msgpack::unpacked result;
            msgpack::unpack(result, (const char*) file.data(), file.size(), 0);
            result.get().convert(syncingMetadatas_);
        } catch (const std::exception& e) {
            JAMI_WARNING("[Account {}] [ConversationModule] unable to load syncing metadata: {}",
                         accountId_,
                         e.what());
        }
    }
};

ConversationModule::Impl::Impl(std::shared_ptr<JamiAccount>&& account,
                               std::shared_ptr<AccountManager>&& accountManager,
                               NeedsSyncingCb&& needsSyncingCb,
                               SengMsgCb&& sendMsgCb,
                               NeedSocketCb&& onNeedSocket,
                               NeedSocketCb&& onNeedSwarmSocket,
                               OneToOneRecvCb&& oneToOneRecvCb)
    : account_(account)
    , accountManager_(accountManager)
    , accountId_(account->getAccountID())
    , needsSyncingCb_(needsSyncingCb)
    , sendMsgCb_(sendMsgCb)
    , onNeedSocket_(onNeedSocket)
    , onNeedSwarmSocket_(onNeedSwarmSocket)
    , oneToOneRecvCb_(oneToOneRecvCb)
{
    if (auto accm = account->accountManager())
        if (const auto* info = accm->getInfo()) {
            deviceId_ = info->deviceId;
            username_ = info->accountId;
        }
    conversationsRequests_ = convRequests(accountId_);
    loadMetadata();
}

void
ConversationModule::Impl::cloneConversation(const std::string& deviceId,
                                            const std::string& peerUri,
                                            const std::string& convId)
{
    JAMI_DEBUG("[Account {}] [Conversation {}] [device {}] Cloning conversation", accountId_, convId, deviceId);

    auto conv = startConversation(convId);
    std::unique_lock lk(conv->mtx);
    cloneConversation(deviceId, peerUri, conv);
}

void
ConversationModule::Impl::cloneConversation(const std::string& deviceId,
                                            const std::string& peerUri,
                                            const std::shared_ptr<SyncedConversation>& conv)
{
    // conv->mtx must be locked
    if (!conv->conversation) {
        // Note: here we don't return and connect to all members
        // the first that will successfully connect will be used for
        // cloning.
        // This avoid the case when we try to clone from convInfos + sync message
        // at the same time.
        if (!conv->startFetch(deviceId, true)) {
            JAMI_WARNING("[Account {}] [Conversation {}] [device {}] Already fetching conversation", accountId_, conv->info.id, deviceId);
            addConvInfo(conv->info);
            return;
        }
        onNeedSocket_(
            conv->info.id,
            deviceId,
            [w = weak(), conv, deviceId](const auto& channel) {
                std::lock_guard lk(conv->mtx);
                if (conv->pending && !conv->pending->ready) {
                    if (channel) {
                        conv->pending->ready = true;
                        conv->pending->deviceId = channel->deviceId().toString();
                        conv->pending->socket = channel;
                        if (!conv->pending->cloning) {
                            conv->pending->cloning = true;
                            dht::ThreadPool::io().run(
                                [w, convId = conv->info.id, deviceId = conv->pending->deviceId]() {
                                    if (auto sthis = w.lock())
                                        sthis->handlePendingConversation(convId, deviceId);
                                });
                        }
                        return true;
                    } else {
                        conv->stopFetch(deviceId);
                    }
                }
                return false;
            },
            MIME_TYPE_GIT);

        JAMI_LOG("[Account {}] [Conversation {}] [device {}] Requesting device",
                 accountId_,
                 conv->info.id,
                 deviceId);
        conv->info.members.emplace(username_);
        conv->info.members.emplace(peerUri);
        addConvInfo(conv->info);
    } else {
        JAMI_DEBUG("[Account {}] [Conversation {}] Conversation already cloned", accountId_, conv->info.id);
    }
}

void
ConversationModule::Impl::fetchNewCommits(const std::string& peer,
                                          const std::string& deviceId,
                                          const std::string& conversationId,
                                          const std::string& commitId)
{
    {
        std::lock_guard lk(convInfosMtx_);
        auto itConv = convInfos_.find(conversationId);
        if (itConv != convInfos_.end() && itConv->second.isRemoved()) {
            // If the conversation is removed and we receives a new commit,
            // it means that the contact was removed but not banned.
            // If he wants a new conversation, they must removes/re-add the contact who declined.
            JAMI_WARNING("[Account {:s}] [Conversation {}] Received a commit, but conversation is removed",
                         accountId_,
                         conversationId);
            return;
        }
    }
    std::optional<ConversationRequest> oldReq;
    {
        std::lock_guard lk(conversationsRequestsMtx_);
        oldReq = getRequest(conversationId);
        if (oldReq != std::nullopt && oldReq->declined) {
            JAMI_DEBUG("[Account {}] [Conversation {}] Received a request for a conversation already declined.",
                       accountId_, conversationId);
            return;
        }
    }
    JAMI_DEBUG("[Account {:s}] [Conversation {}] [device {}] fetching '{:s}'",
               accountId_,
               conversationId,
               deviceId,
               commitId);

    auto conv = getConversation(conversationId);
    if (!conv) {
        if (oldReq == std::nullopt) {
            // We didn't find a conversation or a request with the given ID.
            // This suggests that someone tried to send us an invitation but
            // that we didn't receive it, so we ask for a new one.
            JAMI_WARNING("[Account {}] [Conversation {}] Unable to find conversation, asking for an invite",
                         accountId_,
                         conversationId);
            sendMsgCb_(peer,
                       {},
                       std::map<std::string, std::string> {{MIME_TYPE_INVITE, conversationId}},
                       0);
        }
        return;
    }
    std::unique_lock lk(conv->mtx);

    if (conv->conversation) {
        // Check if we already have the commit
        if (not commitId.empty() && conv->conversation->getCommit(commitId) != std::nullopt) {
            return;
        }
        if (conv->conversation->isRemoving()) {
            JAMI_WARNING("[Account {}] [Conversation {}] conversaton is being removed",
                         accountId_,
                         conversationId);
            return;
        }
        if (!conv->conversation->isMember(peer, true)) {
            JAMI_WARNING("[Account {}] [Conversation {}] {} is not a membe", accountId_, conversationId, peer);
            return;
        }
        if (conv->conversation->isBanned(deviceId)) {
            JAMI_WARNING("[Account {}] [Conversation {}] device {} is banned",
                         accountId_,
                         conversationId,
                         deviceId);
            return;
        }

        // Retrieve current last message
        auto lastMessageId = conv->conversation->lastCommitId();
        if (lastMessageId.empty()) {
            JAMI_ERROR("[Account {}] [Conversation {}] No message detected. This is a bug", accountId_, conversationId);
            return;
        }

        if (!conv->startFetch(deviceId)) {
            JAMI_WARNING("[Account {}] [Conversation {}] Already fetching", accountId_, conversationId);
            return;
        }

        syncCnt.fetch_add(1);
        onNeedSocket_(
            conversationId,
            deviceId,
            [w = weak(),
             conv,
             conversationId = std::move(conversationId),
             peer = std::move(peer),
             deviceId = std::move(deviceId),
             commitId = std::move(commitId)](const auto& channel) {
                auto sthis = w.lock();
                auto acc = sthis ? sthis->account_.lock() : nullptr;
                std::unique_lock lk(conv->mtx);
                auto conversation = conv->conversation;
                if (!channel || !acc || !conversation) {
                    conv->stopFetch(deviceId);
                    if (sthis)
                        sthis->syncCnt.fetch_sub(1);
                    return false;
                }
                conversation->addGitSocket(channel->deviceId(), channel);
                lk.unlock();
                conversation->sync(
                    peer,
                    deviceId,
                    [w,
                     conv,
                     conversationId = std::move(conversationId),
                     peer = std::move(peer),
                     deviceId = std::move(deviceId),
                     commitId = std::move(commitId)](bool ok) {
                        auto shared = w.lock();
                        if (!shared)
                            return;
                        if (!ok) {
                            JAMI_WARNING("[Account {}] [Conversation {}] Unable to fetch new commit from "
                                         "{}, other peer may be disconnected",
                                         shared->accountId_,
                                         conversationId,
                                         deviceId);
                            JAMI_LOG("[Account {}] [Conversation {}] Relaunch sync with {}",
                                     shared->accountId_,
                                     conversationId,
                                     deviceId);
                        }

                        {
                            std::lock_guard lk(conv->mtx);
                            conv->pending.reset();
                            // Notify peers that a new commit is there (DRT)
                            if (not commitId.empty() && ok) {
                                shared->sendMessageNotification(*conv->conversation,
                                                                false,
                                                                commitId,
                                                                deviceId);
                            }
                        }
                        if (shared->syncCnt.fetch_sub(1) == 1) {
                            emitSignal<libjami::ConversationSignal::ConversationSyncFinished>(
                                shared->accountId_);
                        }
                    },
                    commitId);
                return true;
            },
            "");
    } else {
        if (oldReq != std::nullopt)
            return;
        if (conv->pending)
            return;
        bool clone = !conv->info.isRemoved();
        if (clone) {
            cloneConversation(deviceId, peer, conv);
            return;
        }
        lk.unlock();
        JAMI_WARNING("[Account {}] [Conversation {}] Unable to find conversation, asking for an invite",
                     accountId_,
                     conversationId);
        sendMsgCb_(peer,
                   {},
                   std::map<std::string, std::string> {{MIME_TYPE_INVITE, conversationId}},
                   0);
    }
}

// Clone and store conversation
void
ConversationModule::Impl::handlePendingConversation(const std::string& conversationId,
                                                    const std::string& deviceId)
{
    auto acc = account_.lock();
    if (!acc)
        return;
    std::vector<DeviceId> kd;
    {
        std::unique_lock lk(conversationsMtx_);
        const auto& devices = accountManager_->getKnownDevices();
        kd.reserve(devices.size());
        for (const auto& [id, _] : devices)
            kd.emplace_back(id);
    }
    auto conv = getConversation(conversationId);
    if (!conv)
        return;
    std::unique_lock lk(conv->mtx, std::defer_lock);
    auto erasePending = [&] {
        std::string toRm;
        if (conv->pending && !conv->pending->removeId.empty())
            toRm = std::move(conv->pending->removeId);
        conv->pending.reset();
        lk.unlock();
        if (!toRm.empty())
            removeConversation(toRm);
    };
    try {
        auto conversation = std::make_shared<Conversation>(acc, deviceId, conversationId);
        conversation->onMembersChanged([w = weak_from_this(), conversationId](const auto& members) {
            // Delay in another thread to avoid deadlocks
            dht::ThreadPool::io().run([w, conversationId, members = std::move(members)] {
                if (auto sthis = w.lock())
                    sthis->setConversationMembers(conversationId, members);
            });
        });
        conversation->onMessageStatusChanged([this, conversationId](const auto& status) {
            auto msg = std::make_shared<SyncMsg>();
            msg->ms = {{conversationId, status}};
            needsSyncingCb_(std::move(msg));
        });
        conversation->onNeedSocket(onNeedSwarmSocket_);
        if (!conversation->isMember(username_, true)) {
            JAMI_ERROR("[Account {}] [Conversation {}] Conversation cloned but we do not seem to be a valid member",
                accountId_,
                conversationId);
            conversation->erase();
            lk.lock();
            erasePending();
            return;
        }

        // Make sure that the list of members stored in convInfos_ matches the
        // one from the conversation's repository.
        // (https://git.jami.net/savoirfairelinux/jami-daemon/-/issues/1026)
        setConversationMembers(conversationId, conversation->memberUris("", {}));

        lk.lock();

        if (conv->pending && conv->pending->socket)
            conversation->addGitSocket(DeviceId(deviceId), std::move(conv->pending->socket));
        auto removeRepo = false;
        // Note: a removeContact while cloning. In this case, the conversation
        // must not be announced and removed.
        if (conv->info.isRemoved())
            removeRepo = true;
        std::map<std::string, std::string> preferences;
        std::map<std::string, std::map<std::string, std::string>> status;
        if (conv->pending) {
            preferences = std::move(conv->pending->preferences);
            status = std::move(conv->pending->status);
        }
        conv->conversation = conversation;
        if (removeRepo) {
            removeRepositoryImpl(*conv, false, true);
            erasePending();
            return;
        }

        auto commitId = conversation->join();
        std::vector<std::map<std::string, std::string>> messages;
        {
            std::lock_guard lk(replayMtx_);
            auto replayIt = replay_.find(conversationId);
            if (replayIt != replay_.end()) {
                messages = std::move(replayIt->second);
                replay_.erase(replayIt);
            }
        }
        if (!commitId.empty())
            sendMessageNotification(*conversation, false, commitId);
        erasePending(); // Will unlock

#ifdef LIBJAMI_TEST
        conversation->onBootstrapStatus(bootstrapCbTest_);
#endif
        conversation->bootstrap(std::bind(&ConversationModule::Impl::bootstrapCb,
                                          this,
                                          conversation->id()),
                                kd);

        if (!preferences.empty())
            conversation->updatePreferences(preferences);
        if (!status.empty())
            conversation->updateMessageStatus(status);
        syncingMetadatas_.erase(conversationId);
        saveMetadata();

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
                                                                               true,
                                                                               *commits.rbegin());
                                       });
        // Download members profile on first sync
        auto isOneOne = conversation->mode() == ConversationMode::ONE_TO_ONE;
        auto askForProfile = isOneOne;
        if (!isOneOne) {
            // If not 1:1 only download profiles from self (to avoid non checked files)
            auto cert = acc->certStore().getCertificate(deviceId);
            askForProfile = cert && cert->issuer && cert->issuer->getId().toString() == username_;
        }
        if (askForProfile) {
            for (const auto& member : conversation->memberUris(username_)) {
                acc->askForProfile(conversationId, deviceId, member);
            }
        }
    } catch (const std::exception& e) {
        JAMI_WARNING("[Account {}] [Conversation {}] Something went wrong when cloning conversation: {}. Re-clone in {}s",
                     accountId_, 
                     conversationId,
                     e.what(),
                     conv->fallbackTimer.count());
        conv->fallbackClone->expires_at(std::chrono::steady_clock::now() + conv->fallbackTimer);
        conv->fallbackTimer *= 2;
        if (conv->fallbackTimer > MAX_FALLBACK)
            conv->fallbackTimer = MAX_FALLBACK;
        conv->fallbackClone->async_wait(std::bind(&ConversationModule::Impl::fallbackClone,
                                                  shared_from_this(),
                                                  std::placeholders::_1,
                                                  conversationId));
    }
    lk.lock();
    erasePending();
}

std::optional<ConversationRequest>
ConversationModule::Impl::getRequest(const std::string& id) const
{
    // ConversationsRequestsMtx MUST BE LOCKED
    auto it = conversationsRequests_.find(id);
    if (it != conversationsRequests_.end())
        return it->second;
    return std::nullopt;
}

std::string
ConversationModule::Impl::getOneToOneConversation(const std::string& uri) const noexcept
{
    auto details = accountManager_->getContactDetails(uri);
    auto itRemoved = details.find("removed");
    // If contact is removed there is no conversation
    if (itRemoved != details.end() && itRemoved->second != "0") {
        auto itBanned = details.find("banned");
        // If banned, conversation is still on disk
        if (itBanned == details.end() || itBanned->second == "0") {
            // Check if contact is removed
            auto itAdded = details.find("added");
            if (std::stoi(itRemoved->second) > std::stoi(itAdded->second))
                return {};
        }
    }
    auto it = details.find(libjami::Account::TrustRequest::CONVERSATIONID);
    if (it != details.end())
        return it->second;
    return {};
}

bool
ConversationModule::Impl::updateConvForContact(const std::string& uri,
                                               const std::string& oldConv,
                                               const std::string& newConv)
{
    if (newConv != oldConv) {
        auto conversation = getOneToOneConversation(uri);
        if (conversation != oldConv) {
            JAMI_DEBUG("[Account {}] [Conversation {}] Old conversation is not found in details {} - found: {}",
                        accountId_,
                        newConv,
                       oldConv,
                       conversation);
            return false;
        }
        accountManager_->updateContactConversation(uri, newConv);
        return true;
    }
    return false;
}

void
ConversationModule::Impl::declineOtherConversationWith(const std::string& uri) noexcept
{
    // conversationsRequestsMtx_ MUST BE LOCKED
    for (auto& [id, request] : conversationsRequests_) {
        if (request.declined)
            continue; // Ignore already declined requests
        if (request.isOneToOne() && request.from == uri) {
            JAMI_WARNING("[Account {}] [Conversation {}] Decline conversation request from {}",
                            accountId_, id, uri);
            request.declined = std::time(nullptr);
            syncingMetadatas_.erase(id);
            saveMetadata();
            emitSignal<libjami::ConversationSignal::ConversationRequestDeclined>(accountId_, id);
        }
    }
}

std::vector<std::map<std::string, std::string>>
ConversationModule::Impl::getConversationMembers(const std::string& conversationId,
                                                 bool includeBanned) const
{
    return withConv(conversationId,
                    [&](const auto& conv) { return conv.getMembers(true, includeBanned); });
}

void
ConversationModule::Impl::removeRepository(const std::string& conversationId, bool sync, bool force)
{
    auto conv = getConversation(conversationId);
    if (!conv)
        return;
    std::unique_lock lk(conv->mtx);
    removeRepositoryImpl(*conv, sync, force);
}

void
ConversationModule::Impl::removeRepositoryImpl(SyncedConversation& conv, bool sync, bool force)
{
    if (conv.conversation && (force || conv.conversation->isRemoving())) {
        // Stop fetch!
        conv.pending.reset();

        JAMI_LOG("[Account {}] [Conversation {}] Remove conversation", accountId_, conv.info.id);
        try {
            if (conv.conversation->mode() == ConversationMode::ONE_TO_ONE) {
                for (const auto& member : conv.conversation->getInitialMembers()) {
                    if (member != username_) {
                        // Note: this can happen while re-adding a contact.
                        // In this case, check that we are removing the linked conversation.
                        if (conv.info.id == getOneToOneConversation(member)) {
                            accountManager_->removeContactConversation(member);
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            JAMI_ERR() << e.what();
        }
        conv.conversation->erase();
        conv.conversation.reset();

        if (!sync)
            return;

        conv.info.erased = std::time(nullptr);
        needsSyncingCb_({});
        addConvInfo(conv.info);
    }
}

bool
ConversationModule::Impl::removeConversation(const std::string& conversationId)
{
    return withConv(conversationId, [this](auto& conv) { return removeConversationImpl(conv); });
}

bool
ConversationModule::Impl::removeConversationImpl(SyncedConversation& conv)
{
    auto members = conv.getMembers(false, false);
    auto isSyncing = !conv.conversation;
    auto hasMembers = !isSyncing // If syncing there is no member to inform
                      && std::find_if(members.begin(),
                                      members.end(),
                                      [&](const auto& member) {
                                          return member.at("uri") == username_;
                                      })
                             != members.end() // We must be still a member
                      && members.size() != 1; // If there is only ourself
    conv.info.removed = std::time(nullptr);
    if (isSyncing)
        conv.info.erased = std::time(nullptr);
    // Sync now, because it can take some time to really removes the datas
    needsSyncingCb_({});
    addConvInfo(conv.info);
    emitSignal<libjami::ConversationSignal::ConversationRemoved>(accountId_, conv.info.id);
    if (isSyncing)
        return true;
    if (conv.conversation->mode() != ConversationMode::ONE_TO_ONE) {
        // For one to one, we do not notify the leave. The other can still generate request
        // and this is managed by the banned part. If we re-accept, the old conversation will be
        // retrieved
        auto commitId = conv.conversation->leave();
        if (hasMembers) {
            JAMI_LOG("Wait that someone sync that user left conversation {}", conv.info.id);
            // Commit that we left
            if (!commitId.empty()) {
                // Do not sync as it's synched by convInfos
                sendMessageNotification(*conv.conversation, false, commitId);
            } else {
                JAMI_ERROR("Failed to send message to conversation {}", conv.info.id);
            }
            // In this case, we wait that another peer sync the conversation
            // to definitely remove it from the device. This is to inform the
            // peer that we left the conversation and never want to receive
            // any messages
            return true;
        }
    } else {
        for (const auto& m : members)
            if (username_ != m.at("uri"))
                updateConvForContact(m.at("uri"), conv.info.id, "");
    }
    // Else we are the last member, so we can remove
    removeRepositoryImpl(conv, true);
    return true;
}

void
ConversationModule::Impl::sendMessageNotification(const std::string& conversationId,
                                                  bool sync,
                                                  const std::string& commitId,
                                                  const std::string& deviceId)
{
    if (auto conv = getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation)
            sendMessageNotification(*conv->conversation, sync, commitId, deviceId);
    }
}

void
ConversationModule::Impl::sendMessageNotification(Conversation& conversation,
                                                  bool sync,
                                                  const std::string& commitId,
                                                  const std::string& deviceId)
{
    auto acc = account_.lock();
    if (!acc)
        return;
    Json::Value message;
    auto commit = commitId == "" ? conversation.lastCommitId() : commitId;
    message["id"] = conversation.id();
    message["commit"] = commit;
    message["deviceId"] = deviceId_;
    const auto text = json::toString(message);

    // Send message notification will announce the new commit in 3 steps.

    // First, because our account can have several devices, announce to other devices
    if (sync) {
        // Announce to our devices
        refreshMessage[username_] = sendMsgCb_(username_,
                                               {},
                                               std::map<std::string, std::string> {
                                                   {MIME_TYPE_GIT, text}},
                                               refreshMessage[username_]);
    }

    // Then, we announce to 2 random members in the conversation that aren't in the DRT
    // This allow new devices without the ability to sync to their other devices to sync with us.
    // Or they can also use an old backup.
    std::vector<std::string> nonConnectedMembers;
    std::vector<NodeId> devices;
    {
        std::lock_guard lk(notSyncedNotificationMtx_);
        devices = conversation.peersToSyncWith();
        auto members = conversation.memberUris(username_, {MemberRole::BANNED});
        std::vector<std::string> connectedMembers;
        // print all members
        for (const auto& device : devices) {
            auto cert = acc->certStore().getCertificate(device.toString());
            if (cert && cert->issuer)
                connectedMembers.emplace_back(cert->issuer->getId().toString());
        }
        std::sort(std::begin(connectedMembers), std::end(connectedMembers));
        std::set_difference(members.begin(),
                            members.end(),
                            connectedMembers.begin(),
                            connectedMembers.end(),
                            std::inserter(nonConnectedMembers, nonConnectedMembers.begin()));
        std::shuffle(nonConnectedMembers.begin(), nonConnectedMembers.end(), acc->rand);
        if (nonConnectedMembers.size() > 2)
            nonConnectedMembers.resize(2);
        if (!conversation.isBootstraped()) {
            JAMI_DEBUG("[Conversation {}] Not yet bootstraped, save notification",
                       conversation.id());
            // Because we can get some git channels but not bootstraped, we should keep this
            // to refresh when bootstraped.
            notSyncedNotification_[conversation.id()] = commit;
        }
    }

    for (const auto& member : nonConnectedMembers) {
        refreshMessage[member] = sendMsgCb_(member,
                                            {},
                                            std::map<std::string, std::string> {
                                                {MIME_TYPE_GIT, text}},
                                            refreshMessage[member]);
    }

    // Finally we send to devices that the DRT choose.
    for (const auto& device : devices) {
        auto deviceIdStr = device.toString();
        auto memberUri = conversation.uriFromDevice(deviceIdStr);
        if (memberUri.empty() || deviceIdStr == deviceId)
            continue;
        refreshMessage[deviceIdStr] = sendMsgCb_(memberUri,
                                                 device,
                                                 std::map<std::string, std::string> {
                                                     {MIME_TYPE_GIT, text}},
                                                 refreshMessage[deviceIdStr]);
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
    if (auto conv = getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation)
            conv->conversation
                ->sendMessage(std::move(value),
                              replyTo,
                              std::move(onCommit),
                              [this,
                               conversationId,
                               announce,
                               cb = std::move(cb)](bool ok, const std::string& commitId) {
                                  if (cb)
                                      cb(ok, commitId);
                                  if (!announce)
                                      return;
                                  if (ok)
                                      sendMessageNotification(conversationId, true, commitId);
                                  else
                                      JAMI_ERR("Failed to send message to conversation %s",
                                               conversationId.c_str());
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
    std::string type, tid;
    if (auto conv = getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation) {
            auto commit = conv->conversation->getCommit(editedId);
            if (commit != std::nullopt) {
                type = commit->at("type");
                if (type == "application/data-transfer+json")
                    tid = commit->at("tid");
                validCommit = commit->at("author") == username_
                              && (type == "text/plain" || type == "application/data-transfer+json");
            }
        }
    }
    if (!validCommit) {
        JAMI_ERROR("Unable to edit commit {:s}", editedId);
        return;
    }
    // Commit message edition
    Json::Value json;
    if (type == "application/data-transfer+json") {
        json["tid"] = "";
        // Remove file!
        auto path = fileutils::get_data_dir() / accountId_ / "conversation_data" / conversationId
                    / fmt::format("{}_{}", editedId, tid);
        dhtnet::fileutils::remove(path, true);
    } else {
        json["body"] = newBody;
    }
    json["edit"] = editedId;
    json["type"] = type;
    sendMessage(conversationId, std::move(json));
}

void
ConversationModule::Impl::bootstrapCb(std::string convId)
{
    std::string commitId;
    {
        std::lock_guard lk(notSyncedNotificationMtx_);
        auto it = notSyncedNotification_.find(convId);
        if (it != notSyncedNotification_.end()) {
            commitId = it->second;
            notSyncedNotification_.erase(it);
        }
    }
    JAMI_DEBUG("[Account {}] [Conversation {}] Resend last message notification", accountId_, convId);
    dht::ThreadPool::io().run([w = weak(), convId, commitId = std::move(commitId)] {
        if (auto sthis = w.lock())
            sthis->sendMessageNotification(convId, true, commitId);
    });
}

void
ConversationModule::Impl::fixStructures(
    std::shared_ptr<JamiAccount> acc,
    const std::vector<std::tuple<std::string, std::string, std::string>>& updateContactConv,
    const std::set<std::string>& toRm)
{
    for (const auto& [uri, oldConv, newConv] : updateContactConv) {
        updateConvForContact(uri, oldConv, newConv);
    }
    ////////////////////////////////////////////////////////////////
    // Note: This is only to homogenize trust and convRequests
    std::vector<std::string> invalidPendingRequests;
    {
        auto requests = acc->getTrustRequests();
        std::lock_guard lk(conversationsRequestsMtx_);
        for (const auto& request : requests) {
            auto itConvId = request.find(libjami::Account::TrustRequest::CONVERSATIONID);
            auto itConvFrom = request.find(libjami::Account::TrustRequest::FROM);
            if (itConvId != request.end() && itConvFrom != request.end()) {
                // Check if requests exists or is declined.
                auto itReq = conversationsRequests_.find(itConvId->second);
                auto declined = itReq == conversationsRequests_.end() || itReq->second.declined;
                if (declined) {
                    JAMI_WARNING("Invalid trust request found: {:s}", itConvId->second);
                    invalidPendingRequests.emplace_back(itConvFrom->second);
                }
            }
        }
        auto requestRemoved = false;
        for (auto it = conversationsRequests_.begin(); it != conversationsRequests_.end();) {
            if (it->second.from == username_) {
                JAMI_WARNING("Detected request from ourself, this makes no sense. Remove {}",
                             it->first);
                it = conversationsRequests_.erase(it);
            } else {
                ++it;
            }
        }
        if (requestRemoved) {
            saveConvRequests();
        }
    }
    for (const auto& invalidPendingRequest : invalidPendingRequests)
        acc->discardTrustRequest(invalidPendingRequest);

    ////////////////////////////////////////////////////////////////
    for (const auto& conv : toRm) {
        JAMI_ERROR("[Account {}] Remove conversation ({})", accountId_, conv);
        removeConversation(conv);
    }
    JAMI_DEBUG("[Account {}] Conversations loaded!", accountId_);
}

void
ConversationModule::Impl::cloneConversationFrom(const std::shared_ptr<SyncedConversation> conv,
                                                const std::string& deviceId,
                                                const std::string& oldConvId)
{
    std::lock_guard lk(conv->mtx);
    const auto& conversationId = conv->info.id;
    if (!conv->startFetch(deviceId, true)) {
        JAMI_WARNING("[Account {}] [Conversation {}] Already fetching", accountId_, conversationId);
        return;
    }

    onNeedSocket_(
        conversationId,
        deviceId,
        [wthis = weak_from_this(), conv, conversationId, oldConvId, deviceId](const auto& channel) {
            std::lock_guard lk(conv->mtx);
            if (conv->pending && !conv->pending->ready) {
                conv->pending->removeId = oldConvId;
                if (channel) {
                    conv->pending->ready = true;
                    conv->pending->deviceId = channel->deviceId().toString();
                    conv->pending->socket = channel;
                    if (!conv->pending->cloning) {
                        conv->pending->cloning = true;
                        dht::ThreadPool::io().run(
                            [wthis, conversationId, deviceId = conv->pending->deviceId]() {
                                if (auto sthis = wthis.lock())
                                    sthis->handlePendingConversation(conversationId, deviceId);
                            });
                    }
                    return true;
                } else if (auto sthis = wthis.lock()) {
                    conv->stopFetch(deviceId);
                    JAMI_WARNING("[Account {}] [Conversation {}] Clone failed. Re-clone in {}s", sthis->accountId_, conversationId, conv->fallbackTimer.count());
                    conv->fallbackClone->expires_at(std::chrono::steady_clock::now()
                                                    + conv->fallbackTimer);
                    conv->fallbackTimer *= 2;
                    if (conv->fallbackTimer > MAX_FALLBACK)
                        conv->fallbackTimer = MAX_FALLBACK;
                    conv->fallbackClone->async_wait(
                        std::bind(&ConversationModule::Impl::fallbackClone,
                                  sthis,
                                  std::placeholders::_1,
                                  conversationId));
                }
            }
            return false;
        },
        MIME_TYPE_GIT);
}

void
ConversationModule::Impl::fallbackClone(const asio::error_code& ec,
                                        const std::string& conversationId)
{
    if (ec == asio::error::operation_aborted)
        return;
    auto conv = getConversation(conversationId);
    if (!conv || conv->conversation)
        return;
    auto members = getConversationMembers(conversationId);
    for (const auto& member : members)
        if (member.at("uri") != username_)
            cloneConversationFrom(conversationId, member.at("uri"));
}

void
ConversationModule::Impl::bootstrap(const std::string& convId)
{
    std::vector<DeviceId> kd;
    {
        std::unique_lock lk(conversationsMtx_);
        const auto& devices = accountManager_->getKnownDevices();
        kd.reserve(devices.size());
        for (const auto& [id, _] : devices)
            kd.emplace_back(id);
    }
    auto bootstrap = [&](auto& conv) {
        if (conv) {
#ifdef LIBJAMI_TEST
            conv->onBootstrapStatus(bootstrapCbTest_);
#endif
            conv->bootstrap(std::bind(&ConversationModule::Impl::bootstrapCb, this, conv->id()), kd);
        }
    };
    std::vector<std::string> toClone;
    if (convId.empty()) {
        std::lock_guard lk(convInfosMtx_);
        for (const auto& [conversationId, convInfo] : convInfos_) {
            auto conv = getConversation(conversationId);
            if (!conv)
                return;
            if ((!conv->conversation && !conv->info.isRemoved())) {
                // Because we're not tracking contact presence in order to sync now,
                // we need to ask to clone requests when bootstraping all conversations
                // else it can stay syncing
                toClone.emplace_back(conversationId);
            } else if (conv->conversation) {
                bootstrap(conv->conversation);
            }
        }
    } else if (auto conv = getConversation(convId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation)
            bootstrap(conv->conversation);
    }

    for (const auto& cid : toClone) {
        auto members = getConversationMembers(cid);
        for (const auto& member : members) {
            if (member.at("uri") != username_)
                cloneConversationFrom(cid, member.at("uri"));
        }
    }
}

void
ConversationModule::Impl::cloneConversationFrom(const std::string& conversationId,
                                                const std::string& uri,
                                                const std::string& oldConvId)
{
    auto memberHash = dht::InfoHash(uri);
    if (!memberHash) {
        JAMI_WARNING("Invalid member detected: {}", uri);
        return;
    }
    auto conv = startConversation(conversationId);
    std::lock_guard lk(conv->mtx);
    conv->info = {conversationId};
    conv->info.created = std::time(nullptr);
    conv->info.members.emplace(username_);
    conv->info.members.emplace(uri);
    accountManager_->forEachDevice(memberHash,
                                   [w = weak(), conv, conversationId, oldConvId](
                                       const std::shared_ptr<dht::crypto::PublicKey>& pk) {
                                       auto sthis = w.lock();
                                       auto deviceId = pk->getLongId().toString();
                                       if (!sthis or deviceId == sthis->deviceId_)
                                           return;
                                       sthis->cloneConversationFrom(conv, deviceId, oldConvId);
                                   });
    addConvInfo(conv->info);
}

////////////////////////////////////////////////////////////////

void
ConversationModule::saveConvRequests(
    const std::string& accountId,
    const std::map<std::string, ConversationRequest>& conversationsRequests)
{
    auto path = fileutils::get_data_dir() / accountId;
    saveConvRequestsToPath(path, conversationsRequests);
}

void
ConversationModule::saveConvRequestsToPath(
    const std::filesystem::path& path,
    const std::map<std::string, ConversationRequest>& conversationsRequests)
{
    auto p = path / "convRequests";
    std::lock_guard lock(dhtnet::fileutils::getFileLock(p));
    std::ofstream file(p, std::ios::trunc | std::ios::binary);
    msgpack::pack(file, conversationsRequests);
}

void
ConversationModule::saveConvInfos(const std::string& accountId, const ConvInfoMap& conversations)
{
    auto path = fileutils::get_data_dir() / accountId;
    saveConvInfosToPath(path, conversations);
}

void
ConversationModule::saveConvInfosToPath(const std::filesystem::path& path,
                                        const ConvInfoMap& conversations)
{
    std::ofstream file(path / "convInfo", std::ios::trunc | std::ios::binary);
    msgpack::pack(file, conversations);
}

////////////////////////////////////////////////////////////////

ConversationModule::ConversationModule(std::shared_ptr<JamiAccount> account,
                                       std::shared_ptr<AccountManager> accountManager,
                                       NeedsSyncingCb&& needsSyncingCb,
                                       SengMsgCb&& sendMsgCb,
                                       NeedSocketCb&& onNeedSocket,
                                       NeedSocketCb&& onNeedSwarmSocket,
                                       OneToOneRecvCb&& oneToOneRecvCb,
                                       bool autoLoadConversations)
    : pimpl_ {std::make_unique<Impl>(std::move(account),
                                     std::move(accountManager),
                                     std::move(needsSyncingCb),
                                     std::move(sendMsgCb),
                                     std::move(onNeedSocket),
                                     std::move(onNeedSwarmSocket),
                                     std::move(oneToOneRecvCb))}
{
    if (autoLoadConversations) {
        loadConversations();
    }
}

void
ConversationModule::setAccountManager(std::shared_ptr<AccountManager> accountManager)
{
    std::unique_lock lk(pimpl_->conversationsMtx_);
    pimpl_->accountManager_ = accountManager;
}

#ifdef LIBJAMI_TEST
void
ConversationModule::onBootstrapStatus(
    const std::function<void(std::string, Conversation::BootstrapStatus)>& cb)
{
    pimpl_->bootstrapCbTest_ = cb;
    for (auto& c : pimpl_->getConversations())
        c->onBootstrapStatus(pimpl_->bootstrapCbTest_);
}
#endif

void
ConversationModule::loadConversations()
{
    auto acc = pimpl_->account_.lock();
    if (!acc)
        return;
    JAMI_LOG("[Account {}] Start loading conversations…", pimpl_->accountId_);
    auto conversationsRepositories = dhtnet::fileutils::readDirectory(
        fileutils::get_data_dir() / pimpl_->accountId_ / "conversations");

    std::unique_lock lk(pimpl_->conversationsMtx_);
    auto contacts = pimpl_->accountManager_->getContacts(
        true); // Avoid to lock configurationMtx while conv Mtx is locked
    std::unique_lock ilk(pimpl_->convInfosMtx_);
    pimpl_->convInfos_ = convInfos(pimpl_->accountId_);
    pimpl_->conversations_.clear();

    struct Ctx
    {
        std::mutex cvMtx;
        std::condition_variable cv;
        std::mutex toRmMtx;
        std::set<std::string> toRm;
        std::mutex convMtx;
        size_t convNb;
        std::vector<std::map<std::string, std::string>> contacts;
        std::vector<std::tuple<std::string, std::string, std::string>> updateContactConv;
    };
    auto ctx = std::make_shared<Ctx>();
    ctx->convNb = conversationsRepositories.size();
    ctx->contacts = std::move(contacts);

    for (auto&& r : conversationsRepositories) {
        dht::ThreadPool::io().run([this, ctx, repository = std::move(r), acc] {
            try {
                auto sconv = std::make_shared<SyncedConversation>(repository);
                auto conv = std::make_shared<Conversation>(acc, repository);
                conv->onMessageStatusChanged([this, repository](const auto& status) {
                    auto msg = std::make_shared<SyncMsg>();
                    msg->ms = {{repository, status}};
                    pimpl_->needsSyncingCb_(std::move(msg));
                });
                conv->onMembersChanged(
                    [w = pimpl_->weak_from_this(), repository](const auto& members) {
                        // Delay in another thread to avoid deadlocks
                        dht::ThreadPool::io().run([w, repository, members = std::move(members)] {
                            if (auto sthis = w.lock())
                                sthis->setConversationMembers(repository, members);
                        });
                    });
                conv->onNeedSocket(pimpl_->onNeedSwarmSocket_);
                auto members = conv->memberUris(acc->getUsername(), {});
                // NOTE: The following if is here to protect against any incorrect state
                // that can be introduced
                if (conv->mode() == ConversationMode::ONE_TO_ONE && members.size() == 1) {
                    // If we got a 1:1 conversation, but not in the contact details, it's rather a
                    // duplicate or a weird state
                    auto otherUri = *members.begin();
                    auto itContact = std::find_if(ctx->contacts.cbegin(),
                                                  ctx->contacts.cend(),
                                                  [&](const auto& c) {
                                                      return c.at("id") == otherUri;
                                                  });
                    if (itContact == ctx->contacts.end()) {
                        JAMI_WARNING("Contact {} not found", otherUri);
                        std::lock_guard lkCv {ctx->cvMtx};
                        --ctx->convNb;
                        ctx->cv.notify_all();
                        return;
                    }
                    const std::string& convFromDetails = itContact->at("conversationId");
                    auto removed = std::stoul(itContact->at("removed"));
                    auto added = std::stoul(itContact->at("added"));
                    auto isRemoved = removed > added;
                    if (convFromDetails != repository) {
                        if (convFromDetails.empty()) {
                            if (isRemoved) {
                                // If details is empty, contact is removed and not banned.
                                JAMI_ERROR("Conversation {} detected for {} and should be removed",
                                           repository,
                                           otherUri);
                                std::lock_guard lkMtx {ctx->toRmMtx};
                                ctx->toRm.insert(repository);
                            } else {
                                JAMI_ERROR("No conversation detected for {} but one exists ({}). "
                                           "Update details",
                                           otherUri,
                                           repository);
                                std::lock_guard lkMtx {ctx->toRmMtx};
                                ctx->updateContactConv.emplace_back(
                                    std::make_tuple(otherUri, convFromDetails, repository));
                            }
                        } else {
                            JAMI_ERROR("Multiple conversation detected for {} but ({} & {})",
                                       otherUri,
                                       repository,
                                       convFromDetails);
                            std::lock_guard lkMtx {ctx->toRmMtx};
                            ctx->toRm.insert(repository);
                        }
                    }
                }
                {
                    std::lock_guard lkMtx {ctx->convMtx};
                    auto convInfo = pimpl_->convInfos_.find(repository);
                    if (convInfo == pimpl_->convInfos_.end()) {
                        JAMI_ERROR("Missing conv info for {}. This is a bug!", repository);
                        sconv->info.created = std::time(nullptr);
                        sconv->info.lastDisplayed
                            = conv->infos()[ConversationMapKeys::LAST_DISPLAYED];
                    } else {
                        sconv->info = convInfo->second;
                        if (convInfo->second.isRemoved()) {
                            // A conversation was removed, but repository still exists
                            conv->setRemovingFlag();
                            std::lock_guard lkMtx {ctx->toRmMtx};
                            ctx->toRm.insert(repository);
                        }
                    }
                    // Even if we found the conversation in convInfos_, unable to assume that the
                    // list of members stored in `convInfo` is correct
                    // (https://git.jami.net/savoirfairelinux/jami-daemon/-/issues/1025). For this
                    // reason, we always use the list we got from the conversation repository to set
                    // the value of `sconv->info.members`.
                    members.emplace(acc->getUsername());
                    sconv->info.members = std::move(members);
                    // convInfosMtx_ is already locked
                    pimpl_->convInfos_[repository] = sconv->info;
                }
                auto commits = conv->commitsEndedCalls();

                if (!commits.empty()) {
                    // Note: here, this means that some calls were actives while the
                    // daemon finished (can be a crash).
                    // Notify other in the conversation that the call is finished
                    pimpl_->sendMessageNotification(*conv, true, *commits.rbegin());
                }
                sconv->conversation = conv;
                std::lock_guard lkMtx {ctx->convMtx};
                pimpl_->conversations_.emplace(repository, std::move(sconv));
            } catch (const std::logic_error& e) {
                JAMI_WARNING("[Account {}] Conversations not loaded: {}",
                             pimpl_->accountId_,
                             e.what());
            }
            std::lock_guard lkCv {ctx->cvMtx};
            --ctx->convNb;
            ctx->cv.notify_all();
        });
    }

    std::unique_lock lkCv(ctx->cvMtx);
    ctx->cv.wait(lkCv, [&] { return ctx->convNb == 0; });

    // Prune any invalid conversations without members and
    // set the removed flag if needed
    std::set<std::string> removed;
    for (auto itInfo = pimpl_->convInfos_.begin(); itInfo != pimpl_->convInfos_.end();) {
        const auto& info = itInfo->second;
        if (info.members.empty()) {
            itInfo = pimpl_->convInfos_.erase(itInfo);
            continue;
        }
        if (info.isRemoved())
            removed.insert(info.id);
        auto itConv = pimpl_->conversations_.find(info.id);
        if (itConv == pimpl_->conversations_.end()) {
            // convInfos_ can contain a conversation that is not yet cloned
            // so we need to add it there.
            itConv = pimpl_->conversations_
                         .emplace(info.id, std::make_shared<SyncedConversation>(info))
                         .first;
        }
        if (itConv != pimpl_->conversations_.end() && itConv->second && itConv->second->conversation
            && info.isRemoved())
            itConv->second->conversation->setRemovingFlag();
        if (!info.isRemoved() && itConv == pimpl_->conversations_.end()) {
            // In this case, the conversation is not synced and we only know ourself
            if (info.members.size() == 1 && *info.members.begin() == acc->getUsername()) {
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
    pimpl_->saveConvInfos();

    ilk.unlock();
    lk.unlock();

    dht::ThreadPool::io().run([w = pimpl_->weak(),
                               acc,
                               updateContactConv = std::move(ctx->updateContactConv),
                               toRm = std::move(ctx->toRm)]() {
        // Will lock account manager
        if (auto shared = w.lock())
            shared->fixStructures(acc, updateContactConv, toRm);
    });
}

void
ConversationModule::loadSingleConversation(const std::string& convId)
{
    auto acc = pimpl_->account_.lock();
    if (!acc)
        return;
    JAMI_LOG("[Account {}] Start loading conversation {}", pimpl_->accountId_, convId);

    std::unique_lock lk(pimpl_->conversationsMtx_);
    std::unique_lock ilk(pimpl_->convInfosMtx_);
    // Load convInfos to retrieve requests that have been accepted but not yet synchronized.
    pimpl_->convInfos_ = convInfos(pimpl_->accountId_);
    pimpl_->conversations_.clear();

    try {
        auto sconv = std::make_shared<SyncedConversation>(convId);

        auto conv = std::make_shared<Conversation>(acc, convId);

        conv->onNeedSocket(pimpl_->onNeedSwarmSocket_);

        sconv->conversation = conv;
        pimpl_->conversations_.emplace(convId, std::move(sconv));
    } catch (const std::logic_error& e) {
        JAMI_WARNING("[Account {}] Conversations not loaded: {}", pimpl_->accountId_, e.what());
    }

    // Add all other conversations as dummy conversations to indicate their existence so
    // isConversation could detect conversations correctly.
    auto conversationsRepositoryIds = dhtnet::fileutils::readDirectory(
        fileutils::get_data_dir() / pimpl_->accountId_ / "conversations");
    for (auto repositoryId : conversationsRepositoryIds) {
        if (repositoryId != convId) {
            auto conv = std::make_shared<SyncedConversation>(repositoryId);
            pimpl_->conversations_.emplace(repositoryId, conv);
        }
    }

    // Add conversations from convInfos_ so isConversation could detect conversations correctly.
    // This includes conversations that have been accepted but are not yet synchronized.
    for (auto itInfo = pimpl_->convInfos_.begin(); itInfo != pimpl_->convInfos_.end();) {
        const auto& info = itInfo->second;
        if (info.members.empty()) {
            itInfo = pimpl_->convInfos_.erase(itInfo);
            continue;
        }
        auto itConv = pimpl_->conversations_.find(info.id);
        if (itConv == pimpl_->conversations_.end()) {
            // convInfos_ can contain a conversation that is not yet cloned
            // so we need to add it there.
            pimpl_->conversations_.emplace(info.id, std::make_shared<SyncedConversation>(info));
        }
        ++itInfo;
    }

    ilk.unlock();
    lk.unlock();
}

void
ConversationModule::bootstrap(const std::string& convId)
{
    pimpl_->bootstrap(convId);
}

void
ConversationModule::monitor()
{
    for (auto& conv : pimpl_->getConversations())
        conv->monitor();
}

void
ConversationModule::clearPendingFetch()
{
    // Note: This is a workaround. convModule() is kept if account is disabled/re-enabled.
    // iOS uses setAccountActive() a lot, and if for some reason the previous pending fetch
    // is not erased (callback not called), it will block the new messages as it will not
    // sync. The best way to debug this is to get logs from the last ICE connection for
    // syncing the conversation. It may have been killed in some un-expected way avoiding to
    // call the callbacks. This should never happen, but if it's the case, this will allow
    // new messages to be synced correctly.
    for (auto& conv : pimpl_->getSyncedConversations()) {
        std::lock_guard lk(conv->mtx);
        if (conv && conv->pending) {
            JAMI_ERR("This is a bug, seems to still fetch to some device on initializing");
            conv->pending.reset();
        }
    }
}

void
ConversationModule::reloadRequests()
{
    pimpl_->conversationsRequests_ = convRequests(pimpl_->accountId_);
}

std::vector<std::string>
ConversationModule::getConversations() const
{
    std::vector<std::string> result;
    std::lock_guard lk(pimpl_->convInfosMtx_);
    result.reserve(pimpl_->convInfos_.size());
    for (const auto& [key, conv] : pimpl_->convInfos_) {
        if (conv.isRemoved())
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

bool
ConversationModule::updateConvForContact(const std::string& uri,
                                         const std::string& oldConv,
                                         const std::string& newConv)
{
    return pimpl_->updateConvForContact(uri, oldConv, newConv);
}

std::vector<std::map<std::string, std::string>>
ConversationModule::getConversationRequests() const
{
    std::vector<std::map<std::string, std::string>> requests;
    std::lock_guard lk(pimpl_->conversationsRequestsMtx_);
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
    auto oldConv = getOneToOneConversation(uri);
    if (!oldConv.empty() && pimpl_->isConversation(oldConv)) {
        // If there is already an active one to one conversation here, it's an active
        // contact and the contact will reclone this activeConv, so ignore the request
        JAMI_WARNING(
            "Contact is sending a request for a non active conversation. Ignore. They will "
            "clone the old one");
        return;
    }
    std::unique_lock lk(pimpl_->conversationsRequestsMtx_);
    ConversationRequest req;
    req.from = uri;
    req.conversationId = conversationId;
    req.received = std::time(nullptr);
    req.metadatas = ConversationRepository::infosFromVCard(vCard::utils::toMap(
        std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size())));
    auto reqMap = req.toMap();
    if (pimpl_->addConversationRequest(conversationId, std::move(req))) {
        lk.unlock();
        emitSignal<libjami::ConfigurationSignal::IncomingTrustRequest>(pimpl_->accountId_,
                                                                       conversationId,
                                                                       uri,
                                                                       payload,
                                                                       received);
        emitSignal<libjami::ConversationSignal::ConversationRequestReceived>(pimpl_->accountId_,
                                                                             conversationId,
                                                                             reqMap);
        pimpl_->needsSyncingCb_({});
    } else {
        JAMI_DEBUG("[Account {}] Received a request for a conversation "
                   "already existing. Ignore",
                   pimpl_->accountId_);
    }
}

void
ConversationModule::onConversationRequest(const std::string& from, const Json::Value& value)
{
    ConversationRequest req(value);
    auto isOneToOne = req.isOneToOne();
    std::string oldConv;
    if (isOneToOne) {
        oldConv = pimpl_->getOneToOneConversation(from);
    }
    std::unique_lock lk(pimpl_->conversationsRequestsMtx_);
    JAMI_DEBUG("[Account {}] Receive a new conversation request for conversation {} from {}",
               pimpl_->accountId_,
               req.conversationId,
               from);
    auto convId = req.conversationId;

    // Already accepted request, do nothing
    if (pimpl_->isConversation(convId))
        return;
    auto oldReq = pimpl_->getRequest(convId);
    if (oldReq != std::nullopt) {
        JAMI_DEBUG("[Account {}] Received a request for a conversation already existing. "
                   "Ignore. Declined: {}",
                   pimpl_->accountId_,
                   static_cast<int>(oldReq->declined));
        return;
    }

    if (!oldConv.empty()) {
        lk.unlock();
        // Already a conversation with the contact.
        // If there is already an active one to one conversation here, it's an active
        // contact and the contact will reclone this activeConv, so ignore the request
        JAMI_WARNING(
            "Contact is sending a request for a non active conversation. Ignore. They will "
            "clone the old one");
        return;
    }

    req.received = std::time(nullptr);
    req.from = from;
    auto reqMap = req.toMap();
    if (pimpl_->addConversationRequest(convId, std::move(req))) {
        lk.unlock();
        // Note: no need to sync here because other connected devices should receive
        // the same conversation request. Will sync when the conversation will be added
        if (isOneToOne)
            pimpl_->oneToOneRecvCb_(convId, from);
        emitSignal<libjami::ConversationSignal::ConversationRequestReceived>(pimpl_->accountId_,
                                                                             convId,
                                                                             reqMap);
    }
}

std::string
ConversationModule::peerFromConversationRequest(const std::string& convId) const
{
    std::lock_guard lk(pimpl_->conversationsRequestsMtx_);
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
    pimpl_->withConversation(conversationId, [&](auto& conversation) {
        if (!conversation.isMember(from, true)) {
            JAMI_WARNING("{} is asking a new invite for {}, but not a member", from, conversationId);
            return;
        }
        JAMI_LOG("{} is asking a new invite for {}", from, conversationId);
        pimpl_->sendMsgCb_(from, {}, conversation.generateInvitation(), 0);
    });
}

void
ConversationModule::acceptConversationRequest(const std::string& conversationId,
                                              const std::string& deviceId)
{
    // For all conversation members, try to open a git channel with this conversation ID
    std::unique_lock lkCr(pimpl_->conversationsRequestsMtx_);
    auto request = pimpl_->getRequest(conversationId);
    if (request == std::nullopt) {
        lkCr.unlock();
        if (auto conv = pimpl_->getConversation(conversationId)) {
            std::unique_lock lk(conv->mtx);
            if (!conv->conversation) {
                lk.unlock();
                pimpl_->cloneConversationFrom(conv, deviceId);
            }
        }
        JAMI_WARNING("[Account {}] Request not found for conversation {}",
                     pimpl_->accountId_,
                     conversationId);
        return;
    }
    pimpl_->rmConversationRequest(conversationId);
    lkCr.unlock();
    pimpl_->accountManager_->acceptTrustRequest(request->from, true);
    cloneConversationFrom(conversationId, request->from);
}

void
ConversationModule::declineConversationRequest(const std::string& conversationId)
{
    std::lock_guard lk(pimpl_->conversationsRequestsMtx_);
    auto it = pimpl_->conversationsRequests_.find(conversationId);
    if (it != pimpl_->conversationsRequests_.end()) {
        it->second.declined = std::time(nullptr);
        pimpl_->saveConvRequests();
    }
    pimpl_->syncingMetadatas_.erase(conversationId);
    pimpl_->saveMetadata();
    emitSignal<libjami::ConversationSignal::ConversationRequestDeclined>(pimpl_->accountId_,
                                                                         conversationId);
    pimpl_->needsSyncingCb_({});
}

std::string
ConversationModule::startConversation(ConversationMode mode, const dht::InfoHash& otherMember)
{
    auto acc = pimpl_->account_.lock();
    if (!acc)
        return {};
    std::vector<DeviceId> kd;
    for (const auto& [id, _] : acc->getKnownDevices())
        kd.emplace_back(id);
    // Create the conversation object
    std::shared_ptr<Conversation> conversation;
    try {
        conversation = std::make_shared<Conversation>(acc, mode, otherMember.toString());
        auto conversationId = conversation->id();
        conversation->onMessageStatusChanged([this, conversationId](const auto& status) {
            auto msg = std::make_shared<SyncMsg>();
            msg->ms = {{conversationId, status}};
            pimpl_->needsSyncingCb_(std::move(msg));
        });
        conversation->onMembersChanged(
            [w = pimpl_->weak_from_this(), conversationId](const auto& members) {
                // Delay in another thread to avoid deadlocks
                dht::ThreadPool::io().run([w, conversationId, members = std::move(members)] {
                    if (auto sthis = w.lock())
                        sthis->setConversationMembers(conversationId, members);
                });
            });
        conversation->onNeedSocket(pimpl_->onNeedSwarmSocket_);
#ifdef LIBJAMI_TEST
        conversation->onBootstrapStatus(pimpl_->bootstrapCbTest_);
#endif
        conversation->bootstrap(std::bind(&ConversationModule::Impl::bootstrapCb,
                                          pimpl_.get(),
                                          conversationId),
                                kd);
    } catch (const std::exception& e) {
        JAMI_ERROR("[Account {}] Error while generating a conversation {}",
                   pimpl_->accountId_,
                   e.what());
        return {};
    }
    auto convId = conversation->id();
    auto conv = pimpl_->startConversation(convId);
    std::unique_lock lk(conv->mtx);
    conv->info.created = std::time(nullptr);
    conv->info.members.emplace(pimpl_->username_);
    if (otherMember)
        conv->info.members.emplace(otherMember.toString());
    conv->conversation = conversation;
    addConvInfo(conv->info);
    lk.unlock();

    pimpl_->needsSyncingCb_({});
    emitSignal<libjami::ConversationSignal::ConversationReady>(pimpl_->accountId_, convId);
    return convId;
}

void
ConversationModule::cloneConversationFrom(const std::string& conversationId,
                                          const std::string& uri,
                                          const std::string& oldConvId)
{
    pimpl_->cloneConversationFrom(conversationId, uri, oldConvId);
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
ConversationModule::addCallHistoryMessage(const std::string& uri,
                                          uint64_t duration_ms,
                                          const std::string& reason)
{
    auto finalUri = uri.substr(0, uri.find("@ring.dht"));
    finalUri = finalUri.substr(0, uri.find("@jami.dht"));
    auto convId = getOneToOneConversation(finalUri);
    if (!convId.empty()) {
        Json::Value value;
        value["to"] = finalUri;
        value["type"] = "application/call-history+json";
        value["duration"] = std::to_string(duration_ms);
        if (!reason.empty())
            value["reason"] = reason;
        sendMessage(convId, std::move(value));
    }
}

bool
ConversationModule::onMessageDisplayed(const std::string& peer,
                                       const std::string& conversationId,
                                       const std::string& interactionId)
{
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::unique_lock lk(conv->mtx);
        if (auto conversation = conv->conversation) {
            lk.unlock();
            return conversation->setMessageDisplayed(peer, interactionId);
        }
    }
    return false;
}

std::map<std::string, std::map<std::string, std::map<std::string, std::string>>>
ConversationModule::convMessageStatus() const
{
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> messageStatus;
    for (const auto& conv : pimpl_->getConversations()) {
        auto d = conv->messageStatus();
        if (!d.empty())
            messageStatus[conv->id()] = std::move(d);
    }
    return messageStatus;
}

uint32_t
ConversationModule::loadConversationMessages(const std::string& conversationId,
                                             const std::string& fromMessage,
                                             size_t n)
{
    auto acc = pimpl_->account_.lock();
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation) {
            const uint32_t id = std::uniform_int_distribution<uint32_t> {1}(acc->rand);
            LogOptions options;
            options.from = fromMessage;
            options.nbOfCommits = n;
            conv->conversation->loadMessages(
                [accountId = pimpl_->accountId_, conversationId, id](auto&& messages) {
                    emitSignal<libjami::ConversationSignal::ConversationLoaded>(id,
                                                                                accountId,
                                                                                conversationId,
                                                                                messages);
                },
                options);
            return id;
        }
    }
    return 0;
}

void
ConversationModule::clearCache(const std::string& conversationId)
{
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation) {
            conv->conversation->clearCache();
        }
    }
}

uint32_t
ConversationModule::loadConversation(const std::string& conversationId,
                                     const std::string& fromMessage,
                                     size_t n)
{
    auto acc = pimpl_->account_.lock();
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation) {
            const uint32_t id = std::uniform_int_distribution<uint32_t> {1}(acc->rand);
            LogOptions options;
            options.from = fromMessage;
            options.nbOfCommits = n;
            conv->conversation->loadMessages2(
                [accountId = pimpl_->accountId_, conversationId, id](auto&& messages) {
                    emitSignal<libjami::ConversationSignal::SwarmLoaded>(id,
                                                                         accountId,
                                                                         conversationId,
                                                                         messages);
                },
                options);
            return id;
        }
    }
    return 0;
}

uint32_t
ConversationModule::loadConversationUntil(const std::string& conversationId,
                                          const std::string& fromMessage,
                                          const std::string& toMessage)
{
    auto acc = pimpl_->account_.lock();
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation) {
            const uint32_t id = std::uniform_int_distribution<uint32_t> {1}(acc->rand);
            LogOptions options;
            options.from = fromMessage;
            options.to = toMessage;
            options.includeTo = true;
            conv->conversation->loadMessages(
                [accountId = pimpl_->accountId_, conversationId, id](auto&& messages) {
                    emitSignal<libjami::ConversationSignal::ConversationLoaded>(id,
                                                                                accountId,
                                                                                conversationId,
                                                                                messages);
                },
                options);
            return id;
        }
    }
    return 0;
}

uint32_t
ConversationModule::loadSwarmUntil(const std::string& conversationId,
                                   const std::string& fromMessage,
                                   const std::string& toMessage)
{
    auto acc = pimpl_->account_.lock();
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation) {
            const uint32_t id = std::uniform_int_distribution<uint32_t> {}(acc->rand);
            LogOptions options;
            options.from = fromMessage;
            options.to = toMessage;
            options.includeTo = true;
            conv->conversation->loadMessages2(
                [accountId = pimpl_->accountId_, conversationId, id](auto&& messages) {
                    emitSignal<libjami::ConversationSignal::SwarmLoaded>(id,
                                                                         accountId,
                                                                         conversationId,
                                                                         messages);
                },
                options);
            return id;
        }
    }
    return 0;
}

std::shared_ptr<TransferManager>
ConversationModule::dataTransfer(const std::string& conversationId) const
{
    return pimpl_->withConversation(conversationId,
                                    [](auto& conversation) { return conversation.dataTransfer(); });
}

bool
ConversationModule::onFileChannelRequest(const std::string& conversationId,
                                         const std::string& member,
                                         const std::string& fileId,
                                         bool verifyShaSum) const
{
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::filesystem::path path;
        std::string sha3sum;
        std::unique_lock lk(conv->mtx);
        if (!conv->conversation)
            return false;
        if (!conv->conversation->onFileChannelRequest(member, fileId, path, sha3sum))
            return false;

        // Release the lock here to prevent the sha3 calculation from blocking other threads.
        lk.unlock();
        if (!std::filesystem::is_regular_file(path)) {
            JAMI_WARNING("[Account {:s}] [Conversation {}] {:s} asked for non existing file {}",
                         pimpl_->accountId_,
                         conversationId,
                         member,
                         fileId);
            return false;
        }
        // Check that our file is correct before sending
        if (verifyShaSum && sha3sum != fileutils::sha3File(path)) {
            JAMI_WARNING("[Account {:s}] [Conversation {}] {:s} asked for file {:s}, but our version is not "
                         "complete or corrupted",
                         pimpl_->accountId_,
                         conversationId,
                         member,
                         fileId);
            return false;
        }
        return true;
    }
    return false;
}

bool
ConversationModule::downloadFile(const std::string& conversationId,
                                 const std::string& interactionId,
                                 const std::string& fileId,
                                 const std::string& path)
{
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation)
            return conv->conversation->downloadFile(interactionId, fileId, path, "", "");
    }
    return false;
}

void
ConversationModule::syncConversations(const std::string& peer, const std::string& deviceId)
{
    // Sync conversations where peer is member
    std::set<std::string> toFetch;
    std::set<std::string> toClone;
    for (const auto& conv : pimpl_->getSyncedConversations()) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation) {
            if (!conv->conversation->isRemoving() && conv->conversation->isMember(peer, false)) {
                toFetch.emplace(conv->info.id);
            }
        } else if (!conv->info.isRemoved()
                   && std::find(conv->info.members.begin(), conv->info.members.end(), peer)
                          != conv->info.members.end()) {
            // In this case the conversation was never cloned (can be after an import)
            toClone.emplace(conv->info.id);
        }
    }
    for (const auto& cid : toFetch)
        pimpl_->fetchNewCommits(peer, deviceId, cid);
    for (const auto& cid : toClone)
        pimpl_->cloneConversation(deviceId, peer, cid);
    if (pimpl_->syncCnt.load() == 0)
        emitSignal<libjami::ConversationSignal::ConversationSyncFinished>(pimpl_->accountId_);
}

void
ConversationModule::onSyncData(const SyncMsg& msg,
                               const std::string& peerId,
                               const std::string& deviceId)
{
    std::vector<std::string> toClone;
    for (const auto& [key, convInfo] : msg.c) {
        const auto& convId = convInfo.id;
        {
            std::lock_guard lk(pimpl_->conversationsRequestsMtx_);
            pimpl_->rmConversationRequest(convId);
        }

        auto conv = pimpl_->startConversation(convInfo);
        std::unique_lock lk(conv->mtx);
        // Skip outdated info
        if (std::max(convInfo.created, convInfo.removed)
            < std::max(conv->info.created, conv->info.removed))
            continue;
        if (not convInfo.isRemoved()) {
            // If multi devices, it can detect a conversation that was already
            // removed, so just check if the convinfo contains a removed conv
            if (conv->info.removed) {
                if (conv->info.removed >= convInfo.created) {
                    // Only reclone if re-added, else the peer is not synced yet (could be
                    // offline before)
                    continue;
                }
                JAMI_DEBUG("Re-add previously removed conversation {:s}", convId);
            }
            conv->info = convInfo;
            if (!conv->conversation) {
                if (deviceId != "") {
                    pimpl_->cloneConversation(deviceId, peerId, conv);
                } else {
                    // In this case, information is from JAMS
                    // JAMS does not store the conversation itself, so we
                    // must use information to clone the conversation
                    addConvInfo(convInfo);
                    toClone.emplace_back(convId);
                }
            }
        } else {
            if (conv->conversation && !conv->conversation->isRemoving()) {
                emitSignal<libjami::ConversationSignal::ConversationRemoved>(pimpl_->accountId_,
                                                                             convId);
                conv->conversation->setRemovingFlag();
            }
            auto update = false;
            if (!conv->info.removed) {
                update = true;
                conv->info.removed = std::time(nullptr);
            }
            if (convInfo.erased && !conv->info.erased) {
                conv->info.erased = std::time(nullptr);
                pimpl_->addConvInfo(conv->info);
                pimpl_->removeRepositoryImpl(*conv, false);
            } else if (update) {
                pimpl_->addConvInfo(conv->info);
            }
        }
    }

    for (const auto& cid : toClone) {
        auto members = getConversationMembers(cid);
        for (const auto& member : members) {
            if (member.at("uri") != pimpl_->username_)
                cloneConversationFrom(cid, member.at("uri"));
        }
    }

    for (const auto& [convId, req] : msg.cr) {
        if (req.from == pimpl_->username_) {
            JAMI_WARNING("Detected request from ourself, ignore {}.", convId);
            continue;
        }
        std::unique_lock lk(pimpl_->conversationsRequestsMtx_);
        if (pimpl_->isConversation(convId)) {
            // Already handled request
            pimpl_->rmConversationRequest(convId);
            continue;
        }

        // New request
        if (!pimpl_->addConversationRequest(convId, req))
            continue;
        lk.unlock();

        if (req.declined != 0) {
            // Request declined
            JAMI_LOG("[Account {:s}] Declined request detected for conversation {:s} (device {:s})",
                     pimpl_->accountId_,
                     convId,
                     deviceId);
            pimpl_->syncingMetadatas_.erase(convId);
            pimpl_->saveMetadata();
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
        if (auto conv = pimpl_->getConversation(convId)) {
            std::unique_lock lk(conv->mtx);
            if (conv->conversation) {
                auto conversation = conv->conversation;
                lk.unlock();
                conversation->updatePreferences(p);
            } else if (conv->pending) {
                conv->pending->preferences = p;
            }
        }
    }

    // Updates displayed for conversations
    for (const auto& [convId, ms] : msg.ms) {
        if (auto conv = pimpl_->getConversation(convId)) {
            std::unique_lock lk(conv->mtx);
            if (conv->conversation) {
                auto conversation = conv->conversation;
                lk.unlock();
                conversation->updateMessageStatus(ms);
            } else if (conv->pending) {
                conv->pending->status = ms;
            }
        }
    }
}

bool
ConversationModule::needsSyncingWith(const std::string& memberUri, const std::string& deviceId) const
{
    // Check if a conversation needs to fetch remote or to be cloned
    std::lock_guard lk(pimpl_->conversationsMtx_);
    for (const auto& [key, ci] : pimpl_->conversations_) {
        std::lock_guard lk(ci->mtx);
        if (ci->conversation) {
            if (ci->conversation->isRemoving() && ci->conversation->isMember(memberUri, false))
                return true;
        } else if (!ci->info.removed
                   && std::find(ci->info.members.begin(), ci->info.members.end(), memberUri)
                          != ci->info.members.end()) {
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
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation) {
            bool remove = conv->conversation->isRemoving();
            conv->conversation->hasFetched(deviceId, commitId);
            if (remove)
                pimpl_->removeRepositoryImpl(*conv, true);
        }
    }
}

void
ConversationModule::fetchNewCommits(const std::string& peer,
                                    const std::string& deviceId,
                                    const std::string& conversationId,
                                    const std::string& commitId)
{
    pimpl_->fetchNewCommits(peer, deviceId, conversationId, commitId);
}

void
ConversationModule::addConversationMember(const std::string& conversationId,
                                          const dht::InfoHash& contactUri,
                                          bool sendRequest)
{
    auto conv = pimpl_->getConversation(conversationId);
    if (not conv || not conv->conversation) {
        JAMI_ERROR("Conversation {:s} does not exist", conversationId);
        return;
    }
    std::unique_lock lk(conv->mtx);

    auto contactUriStr = contactUri.toString();
    if (conv->conversation->isMember(contactUriStr, true)) {
        JAMI_DEBUG("{:s} is already a member of {:s}, resend invite", contactUriStr, conversationId);
        // Note: This should not be necessary, but if for whatever reason the other side didn't
        // join we should not forbid new invites
        auto invite = conv->conversation->generateInvitation();
        lk.unlock();
        pimpl_->sendMsgCb_(contactUriStr, {}, std::move(invite), 0);
        return;
    }

    conv->conversation->addMember(
        contactUriStr,
        [this, conv, conversationId, sendRequest, contactUriStr](bool ok,
                                                                 const std::string& commitId) {
            if (ok) {
                std::unique_lock lk(conv->mtx);
                pimpl_->sendMessageNotification(*conv->conversation,
                                                true,
                                                commitId); // For the other members
                if (sendRequest) {
                    auto invite = conv->conversation->generateInvitation();
                    lk.unlock();
                    pimpl_->sendMsgCb_(contactUriStr, {}, std::move(invite), 0);
                }
            }
        });
}

void
ConversationModule::removeConversationMember(const std::string& conversationId,
                                             const dht::InfoHash& contactUri,
                                             bool isDevice)
{
    auto contactUriStr = contactUri.toString();
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation)
            return conv->conversation
                ->removeMember(contactUriStr,
                               isDevice,
                               [this, conversationId](bool ok, const std::string& commitId) {
                                   if (ok) {
                                       pimpl_->sendMessageNotification(conversationId,
                                                                       true,
                                                                       commitId);
                                   }
                               });
    }
}

std::vector<std::map<std::string, std::string>>
ConversationModule::getConversationMembers(const std::string& conversationId,
                                           bool includeBanned) const
{
    return pimpl_->getConversationMembers(conversationId, includeBanned);
}

uint32_t
ConversationModule::countInteractions(const std::string& convId,
                                      const std::string& toId,
                                      const std::string& fromId,
                                      const std::string& authorUri) const
{
    if (auto conv = pimpl_->getConversation(convId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation)
            return conv->conversation->countInteractions(toId, fromId, authorUri);
    }
    return 0;
}

void
ConversationModule::search(uint32_t req, const std::string& convId, const Filter& filter) const
{
    if (convId.empty()) {
        auto convs = pimpl_->getConversations();
        if (convs.empty()) {
            emitSignal<libjami::ConversationSignal::MessagesFound>(
                req,
                pimpl_->accountId_,
                std::string {},
                std::vector<std::map<std::string, std::string>> {});
            return;
        }
        auto finishedFlag = std::make_shared<std::atomic_int>(convs.size());
        for (const auto& conv : convs) {
            conv->search(req, filter, finishedFlag);
        }
    } else if (auto conv = pimpl_->getConversation(convId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation)
            conv->conversation->search(req, filter, std::make_shared<std::atomic_int>(1));
    }
}

void
ConversationModule::updateConversationInfos(const std::string& conversationId,
                                            const std::map<std::string, std::string>& infos,
                                            bool sync)
{
    auto conv = pimpl_->getConversation(conversationId);
    if (not conv or not conv->conversation) {
        JAMI_ERROR("Conversation {:s} does not exist", conversationId);
        return;
    }
    std::lock_guard lk(conv->mtx);
    conv->conversation
        ->updateInfos(infos, [this, conversationId, sync](bool ok, const std::string& commitId) {
            if (ok && sync) {
                pimpl_->sendMessageNotification(conversationId, true, commitId);
            } else if (sync)
                JAMI_WARNING("Unable to update info on {:s}", conversationId);
        });
}

std::map<std::string, std::string>
ConversationModule::conversationInfos(const std::string& conversationId) const
{
    {
        std::lock_guard lk(pimpl_->conversationsRequestsMtx_);
        auto itReq = pimpl_->conversationsRequests_.find(conversationId);
        if (itReq != pimpl_->conversationsRequests_.end())
            return itReq->second.metadatas;
    }
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        std::map<std::string, std::string> md;
        {
            auto syncingMetadatasIt = pimpl_->syncingMetadatas_.find(conversationId);
            if (syncingMetadatasIt != pimpl_->syncingMetadatas_.end()) {
                if (conv->conversation) {
                    pimpl_->syncingMetadatas_.erase(syncingMetadatasIt);
                    pimpl_->saveMetadata();
                } else {
                    md = syncingMetadatasIt->second;
                }
            }
        }
        if (conv->conversation)
            return conv->conversation->infos();
        else
            return md;
    }
    JAMI_ERROR("Conversation {:s} does not exist", conversationId);
    return {};
}

void
ConversationModule::setConversationPreferences(const std::string& conversationId,
                                               const std::map<std::string, std::string>& prefs)
{
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::unique_lock lk(conv->mtx);
        if (not conv->conversation) {
            JAMI_ERROR("Conversation {:s} does not exist", conversationId);
            return;
        }
        auto conversation = conv->conversation;
        lk.unlock();
        conversation->updatePreferences(prefs);
        auto msg = std::make_shared<SyncMsg>();
        msg->p = {{conversationId, conversation->preferences(true)}};
        pimpl_->needsSyncingCb_(std::move(msg));
    }
}

std::map<std::string, std::string>
ConversationModule::getConversationPreferences(const std::string& conversationId,
                                               bool includeCreated) const
{
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation)
            return conv->conversation->preferences(includeCreated);
    }
    return {};
}

std::map<std::string, std::map<std::string, std::string>>
ConversationModule::convPreferences() const
{
    std::map<std::string, std::map<std::string, std::string>> p;
    for (const auto& conv : pimpl_->getConversations()) {
        auto prefs = conv->preferences(true);
        if (!prefs.empty())
            p[conv->id()] = std::move(prefs);
    }
    return p;
}

std::vector<uint8_t>
ConversationModule::conversationVCard(const std::string& conversationId) const
{
    if (auto conv = pimpl_->getConversation(conversationId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation)
            return conv->conversation->vCard();
    }
    JAMI_ERROR("Conversation {:s} does not exist", conversationId);
    return {};
}

bool
ConversationModule::isBanned(const std::string& convId, const std::string& uri) const
{
    if (auto conv = pimpl_->getConversation(convId)) {
        std::lock_guard lk(conv->mtx);
        if (!conv->conversation)
            return true;
        if (conv->conversation->mode() != ConversationMode::ONE_TO_ONE)
            return conv->conversation->isBanned(uri);
    }
    // If 1:1 we check the certificate status
    std::lock_guard lk(pimpl_->conversationsMtx_);
    return pimpl_->accountManager_->getCertificateStatus(uri)
           == dhtnet::tls::TrustStore::PermissionStatus::BANNED;
}

void
ConversationModule::removeContact(const std::string& uri, bool banned)
{
    // Remove linked conversation's requests
    {
        std::lock_guard lk(pimpl_->conversationsRequestsMtx_);
        auto update = false;
        for (auto it = pimpl_->conversationsRequests_.begin();
             it != pimpl_->conversationsRequests_.end();
             ++it) {
            if (it->second.from == uri && !it->second.declined) {
                JAMI_DEBUG("Declining conversation request {:s} from {:s}", it->first, uri);
                pimpl_->syncingMetadatas_.erase(it->first);
                pimpl_->saveMetadata();
                emitSignal<libjami::ConversationSignal::ConversationRequestDeclined>(
                    pimpl_->accountId_, it->first);
                update = true;
                it->second.declined = std::time(nullptr);
            }
        }
        if (update) {
            pimpl_->saveConvRequests();
            pimpl_->needsSyncingCb_({});
        }
    }
    if (banned) {
        auto conversationId = getOneToOneConversation(uri);
        pimpl_->withConversation(conversationId, [&](auto& conv) { conv.shutdownConnections(); });
        return; // Keep the conversation in banned model but stop connections
    }

    // Removed contacts should not be linked to any conversation
    pimpl_->accountManager_->updateContactConversation(uri, "");

    // Remove all one-to-one conversations with the removed contact
    auto isSelf = uri == pimpl_->username_;
    std::vector<std::string> toRm;
    auto removeConvInfo = [&](const auto& conv, const auto& members) {
        if ((isSelf && members.size() == 1)
            || (!isSelf && std::find(members.begin(), members.end(), uri) != members.end())) {
            // Mark the conversation as removed if it wasn't already
            if (!conv->info.isRemoved()) {
                conv->info.removed = std::time(nullptr);
                emitSignal<libjami::ConversationSignal::ConversationRemoved>(pimpl_->accountId_,
                                                                             conv->info.id);
                pimpl_->addConvInfo(conv->info);
                return true;
            }
        }
        return false;
    };
    {
        std::lock_guard lk(pimpl_->conversationsMtx_);
        for (auto& [convId, conv] : pimpl_->conversations_) {
            std::lock_guard lk(conv->mtx);
            if (conv->conversation) {
                try {
                    // Note it's important to check getUsername(), else
                    // removing self can remove all conversations
                    if (conv->conversation->mode() == ConversationMode::ONE_TO_ONE) {
                        auto initMembers = conv->conversation->getInitialMembers();
                        if (removeConvInfo(conv, initMembers))
                            toRm.emplace_back(convId);
                    }
                } catch (const std::exception& e) {
                    JAMI_WARN("%s", e.what());
                }
            } else {
                removeConvInfo(conv, conv->info.members);
            }
        }
    }
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
    if (auto conv = pimpl_->getConversation(oldConvId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation) {
            std::promise<bool> waitLoad;
            std::future<bool> fut = waitLoad.get_future();
            // we should wait for loadMessage, because it will be deleted after this.
            conv->conversation->loadMessages(
                [&](auto&& messages) {
                    std::reverse(messages.begin(),
                                 messages.end()); // Log is inverted as we want to replay
                    std::lock_guard lk(pimpl_->replayMtx_);
                    pimpl_->replay_[newConvId] = std::move(messages);
                    waitLoad.set_value(true);
                },
                {});
            fut.wait();
        }
    }
}

bool
ConversationModule::isHosting(const std::string& conversationId, const std::string& confId) const
{
    if (conversationId.empty()) {
        std::lock_guard lk(pimpl_->conversationsMtx_);
        return std::find_if(pimpl_->conversations_.cbegin(),
                            pimpl_->conversations_.cend(),
                            [&](const auto& conv) {
                                return conv.second->conversation
                                       && conv.second->conversation->isHosting(confId);
                            })
               != pimpl_->conversations_.cend();
    } else if (auto conv = pimpl_->getConversation(conversationId)) {
        if (conv->conversation) {
            return conv->conversation->isHosting(confId);
        }
    }
    return false;
}

std::vector<std::map<std::string, std::string>>
ConversationModule::getActiveCalls(const std::string& conversationId) const
{
    return pimpl_->withConversation(conversationId, [](const auto& conversation) {
        return conversation.currentCalls();
    });
}

std::shared_ptr<SIPCall>
ConversationModule::call(
    const std::string& url,
    const std::vector<libjami::MediaMap>& mediaList,
    std::function<void(const std::string&, const DeviceId&, const std::shared_ptr<SIPCall>&)>&& cb)
{
    std::string conversationId = "", confId = "", uri = "", deviceId = "";
    if (url.find('/') == std::string::npos) {
        conversationId = url;
    } else {
        auto parameters = jami::split_string(url, '/');
        if (parameters.size() != 4) {
            JAMI_ERROR("Incorrect url {:s}", url);
            return {};
        }
        conversationId = parameters[0];
        uri = parameters[1];
        deviceId = parameters[2];
        confId = parameters[3];
    }

    auto conv = pimpl_->getConversation(conversationId);
    if (!conv)
        return {};
    std::unique_lock lk(conv->mtx);
    if (!conv->conversation) {
        JAMI_ERROR("Conversation {:s} not found", conversationId);
        return {};
    }

    // Check if we want to join a specific conference
    // So, if confId is specified or if there is some activeCalls
    // or if we are the default host.
    auto activeCalls = conv->conversation->currentCalls();
    auto infos = conv->conversation->infos();
    auto itRdvAccount = infos.find("rdvAccount");
    auto itRdvDevice = infos.find("rdvDevice");
    auto sendCallRequest = false;
    if (!confId.empty()) {
        sendCallRequest = true;
        JAMI_DEBUG("Calling self, join conference");
    } else if (!activeCalls.empty()) {
        // Else, we try to join active calls
        sendCallRequest = true;
        auto& ac = *activeCalls.rbegin();
        confId = ac.at("id");
        uri = ac.at("uri");
        deviceId = ac.at("device");
    } else if (itRdvAccount != infos.end() && itRdvDevice != infos.end()
               && !itRdvAccount->second.empty()) {
        // Else, creates "to" (accountId/deviceId/conversationId/confId) and ask remote host
        sendCallRequest = true;
        uri = itRdvAccount->second;
        deviceId = itRdvDevice->second;
        confId = "0";
        JAMI_DEBUG("Remote host detected. Calling {:s} on device {:s}", uri, deviceId);
    }
    lk.unlock();

    auto account = pimpl_->account_.lock();
    std::vector<libjami::MediaMap> mediaMap
        = mediaList.empty() ? MediaAttribute::mediaAttributesToMediaMaps(
              pimpl_->account_.lock()->createDefaultMediaList(
                  pimpl_->account_.lock()->isVideoEnabled()))
                            : mediaList;

    if (!sendCallRequest || (uri == pimpl_->username_ && deviceId == pimpl_->deviceId_)) {
        confId = confId == "0" ? Manager::instance().callFactory.getNewCallID() : confId;
        // TODO attach host with media list
        hostConference(conversationId, confId, "", mediaMap);
        return {};
    }

    // Else we need to create a call
    auto& manager = Manager::instance();
    std::shared_ptr<SIPCall> call = manager.callFactory.newSipCall(account,
                                                                   Call::CallType::OUTGOING,
                                                                   mediaMap);

    if (not call)
        return {};

    auto callUri = fmt::format("{}/{}/{}/{}", conversationId, uri, deviceId, confId);
    account->getIceOptions([call,
                            accountId = account->getAccountID(),
                            callUri,
                            uri = std::move(uri),
                            conversationId,
                            deviceId,
                            cb = std::move(cb)](auto&& opts) {
        if (call->isIceEnabled()) {
            if (not call->createIceMediaTransport(false)
                or not call->initIceMediaTransport(true,
                                                   std::forward<dhtnet::IceTransportOptions>(opts))) {
                return;
            }
        }
        JAMI_DEBUG("New outgoing call with {}", uri);
        call->setPeerNumber(uri);
        call->setPeerUri("swarm:" + uri);

        JAMI_DEBUG("Calling: {:s}", callUri);
        call->setState(Call::ConnectionState::TRYING);
        call->setPeerNumber(callUri);
        call->setPeerUri("rdv:" + callUri);
        call->addStateListener([accountId, conversationId](Call::CallState call_state,
                                                           Call::ConnectionState cnx_state,
                                                           int) {
            if (cnx_state == Call::ConnectionState::DISCONNECTED
                && call_state == Call::CallState::MERROR) {
                emitSignal<libjami::ConfigurationSignal::NeedsHost>(accountId, conversationId);
                return true;
            }
            return true;
        });
        cb(callUri, DeviceId(deviceId), call);
    });

    return call;
}

void
ConversationModule::hostConference(const std::string& conversationId,
                                   const std::string& confId,
                                   const std::string& callId,
                                   const std::vector<libjami::MediaMap>& mediaList)
{
    auto acc = pimpl_->account_.lock();
    if (!acc)
        return;
    auto conf = acc->getConference(confId);
    auto createConf = !conf;
    std::shared_ptr<SIPCall> call;
    if (!callId.empty()) {
        call = std::dynamic_pointer_cast<SIPCall>(acc->getCall(callId));
        if (!call) {
            JAMI_WARNING("No call with id {} found", callId);
            return;
        }
    }
    if (createConf) {
        conf = std::make_shared<Conference>(acc, confId);
        acc->attach(conf);
    }

    if (!callId.empty())
        conf->addSubCall(callId);

    if (callId.empty())
        conf->attachHost(mediaList);

    if (createConf) {
        emitSignal<libjami::CallSignal::ConferenceCreated>(acc->getAccountID(),
                                                           conversationId,
                                                           conf->getConfId());
    } else {
        conf->reportMediaNegotiationStatus();
        emitSignal<libjami::CallSignal::ConferenceChanged>(acc->getAccountID(),
                                                           conf->getConfId(),
                                                           conf->getStateStr());
        return;
    }

    auto conv = pimpl_->getConversation(conversationId);
    if (!conv)
        return;
    std::unique_lock lk(conv->mtx);
    if (!conv->conversation) {
        JAMI_ERROR("Conversation {} not found", conversationId);
        return;
    }
    // Add commit to conversation
    Json::Value value;
    value["uri"] = pimpl_->username_;
    value["device"] = pimpl_->deviceId_;
    value["confId"] = conf->getConfId();
    value["type"] = "application/call-history+json";
    conv->conversation->hostConference(std::move(value),
                                       [w = pimpl_->weak(),
                                        conversationId](bool ok, const std::string& commitId) {
                                           if (ok) {
                                               if (auto shared = w.lock())
                                                   shared->sendMessageNotification(conversationId,
                                                                                   true,
                                                                                   commitId);
                                           } else {
                                               JAMI_ERR("Failed to send message to conversation %s",
                                                        conversationId.c_str());
                                           }
                                       });

    // When conf finished = remove host & commit
    // Master call, so when it's stopped, the conference will be stopped (as we use the hold
    // state for detaching the call)
    conf->onShutdown([w = pimpl_->weak(),
                      accountUri = pimpl_->username_,
                      confId = conf->getConfId(),
                      conversationId,
                      conv](int duration) {
        auto shared = w.lock();
        if (shared) {
            Json::Value value;
            value["uri"] = accountUri;
            value["device"] = shared->deviceId_;
            value["confId"] = confId;
            value["type"] = "application/call-history+json";
            value["duration"] = std::to_string(duration);

            std::lock_guard lk(conv->mtx);
            if (!conv->conversation) {
                JAMI_ERROR("Conversation {} not found", conversationId);
                return;
            }
            conv->conversation->removeActiveConference(
                std::move(value), [w, conversationId](bool ok, const std::string& commitId) {
                    if (ok) {
                        if (auto shared = w.lock()) {
                            shared->sendMessageNotification(conversationId, true, commitId);
                        }
                    } else {
                        JAMI_ERROR("Failed to send message to conversation {}", conversationId);
                    }
                });
        }
    });
}

std::map<std::string, ConvInfo>
ConversationModule::convInfos(const std::string& accountId)
{
    return convInfosFromPath(fileutils::get_data_dir() / accountId);
}

std::map<std::string, ConvInfo>
ConversationModule::convInfosFromPath(const std::filesystem::path& path)
{
    std::map<std::string, ConvInfo> convInfos;
    try {
        // read file
        std::lock_guard lock(dhtnet::fileutils::getFileLock(path / "convInfo"));
        auto file = fileutils::loadFile("convInfo", path);
        // load values
        msgpack::unpacked result;
        msgpack::unpack(result, (const char*) file.data(), file.size());
        result.get().convert(convInfos);
    } catch (const std::exception& e) {
        JAMI_WARN("[convInfo] error loading convInfo: %s", e.what());
    }
    return convInfos;
}

std::map<std::string, ConversationRequest>
ConversationModule::convRequests(const std::string& accountId)
{
    auto path = fileutils::get_data_dir() / accountId;
    return convRequestsFromPath(path.string());
}

std::map<std::string, ConversationRequest>
ConversationModule::convRequestsFromPath(const std::filesystem::path& path)
{
    std::map<std::string, ConversationRequest> convRequests;
    try {
        // read file
        std::lock_guard lock(dhtnet::fileutils::getFileLock(path / "convRequests"));
        auto file = fileutils::loadFile("convRequests", path);
        // load values
        msgpack::unpacked result;
        msgpack::unpack(result, (const char*) file.data(), file.size(), 0);
        result.get().convert(convRequests);
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
ConversationModule::Impl::setConversationMembers(const std::string& convId,
                                                 const std::set<std::string>& members)
{
    if (auto conv = getConversation(convId)) {
        std::lock_guard lk(conv->mtx);
        conv->info.members = members;
        addConvInfo(conv->info);
    }
}

std::shared_ptr<Conversation>
ConversationModule::getConversation(const std::string& convId)
{
    if (auto conv = pimpl_->getConversation(convId)) {
        std::lock_guard lk(conv->mtx);
        return conv->conversation;
    }
    return nullptr;
}

std::shared_ptr<dhtnet::ChannelSocket>
ConversationModule::gitSocket(std::string_view deviceId, std::string_view convId) const
{
    if (auto conv = pimpl_->getConversation(convId)) {
        std::lock_guard lk(conv->mtx);
        if (conv->conversation)
            return conv->conversation->gitSocket(DeviceId(deviceId));
        else if (conv->pending)
            return conv->pending->socket;
    }
    return nullptr;
}

void
ConversationModule::addGitSocket(std::string_view deviceId,
                                 std::string_view convId,
                                 const std::shared_ptr<dhtnet::ChannelSocket>& channel)
{
    if (auto conv = pimpl_->getConversation(convId)) {
        std::lock_guard lk(conv->mtx);
        conv->conversation->addGitSocket(DeviceId(deviceId), channel);
    } else
        JAMI_WARNING("addGitSocket: Unable to find conversation {:s}", convId);
}

void
ConversationModule::removeGitSocket(std::string_view deviceId, std::string_view convId)
{
    pimpl_->withConversation(convId, [&](auto& conv) { conv.removeGitSocket(DeviceId(deviceId)); });
}

void
ConversationModule::shutdownConnections()
{
    for (const auto& c : pimpl_->getSyncedConversations()) {
        std::lock_guard lkc(c->mtx);
        if (c->conversation)
            c->conversation->shutdownConnections();
        if (c->pending)
            c->pending->socket = {};
    }
}
void
ConversationModule::addSwarmChannel(const std::string& conversationId,
                                    std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    pimpl_->withConversation(conversationId,
                             [&](auto& conv) { conv.addSwarmChannel(std::move(channel)); });
}

void
ConversationModule::connectivityChanged()
{
    for (const auto& conv : pimpl_->getConversations())
        conv->connectivityChanged();
}

std::shared_ptr<Typers>
ConversationModule::getTypers(const std::string& convId)
{
    if (auto c = pimpl_->getConversation(convId)) {
        std::lock_guard lk(c->mtx);
        if (c->conversation)
            return c->conversation->typers();
    }
    return nullptr;
}

} // namespace jami
