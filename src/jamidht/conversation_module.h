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

#include "scheduled_executor.h"
#include "jamidht/conversation.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/jami_contact.h"

#include <mutex>
#include <msgpack.hpp>

namespace jami {

struct SyncMsg
{
    jami::DeviceSync ds;
    std::map<std::string, jami::ConvInfo> c;
    std::map<std::string, jami::ConversationRequest> cr;
    MSGPACK_DEFINE(ds, c, cr)
};

struct PendingConversationFetch // TODO move in pimpl
{
    bool ready {false};
    std::string deviceId {};
};

using ChannelCb = std::function<bool(const std::shared_ptr<ChannelSocket>&)>;
using OnNeedGitCb = std::function<void(const std::string&, const std::string&, ChannelCb&&)>;

class ConversationModule
    : public std::enable_shared_from_this<ConversationModule> // TODO Module interface
{
public:
    ConversationModule(
        std::weak_ptr<JamiAccount>&& account_,
        std::function<void()>&& needsSyncingCb,
        std::function<void(const std::string&, std::map<std::string, std::string>&&)>&& sendMsgCb,
        OnNeedGitCb&& onNeedGitSocket);
    void loadConversations();
    ~ConversationModule() = default;

    std::vector<std::string> getConversations() const;
    std::vector<std::map<std::string, std::string>> getConversationRequests() const;

    void declineConversationRequest(const std::string& conversationId);

    /**
     * Clone a conversation (initial) from device
     * @param deviceId
     * @param convId
     */
    void cloneConversation(const std::string& deviceId,
                           const std::string& peer,
                           const std::string& convId);

    /**
     * Pull remote device (do not do it if commitId is already in the current repo)
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
     * Sync conversations with detected peer
     */
    void syncConversations(const std::string& peer, const std::string& deviceId);

    void setFetched(const std::string& conversationId, const std::string& deviceId);

    void onNewGitCommit(const std::string& peer,
                        const std::string& deviceId,
                        const std::string& conversationId,
                        const std::string& commitId);

    void removeContact(const std::string& uri, bool ban);
    bool removeConversation(const std::string& conversationId);
    /**
     * Remove a repository and all files
     * @param convId
     * @param sync      If we send an update to other account's devices
     * @param force     True if ignore the removing flag
     */
    void removeRepository(const std::string& convId, bool sync, bool force = false);

    void onTrustRequest(const std::string& uri,
                        const std::string& conversationId,
                        const std::vector<uint8_t>& payload,
                        time_t received);

    void checkIfRemoveForCompat(const std::string& peerUri);

    bool needsSyncingWith(const std::string& memberUri, const std::string& deviceId) const;

    bool isBannedDevice(const std::string& convId, const std::string& deviceId) const;

    // Invites
    void onConversationRequest(const std::string& from, const Json::Value&);
    void onNeedConversationRequest(const std::string& from, const std::string& conversationId);

    std::string startConversation(ConversationMode mode = ConversationMode::INVITES_ONLY,
                                  const std::string& otherMember = "");

    uint32_t loadConversationMessages(const std::string& conversationId,
                                      const std::string& fromMessage = "",
                                      size_t n = 0);

    /**
     * Choose if we can accept channel request
     * @param member        member to check
     * @param fileId        file transfer to check (needs to be waiting)
     * @param verifyShaSum  for debug only
     * @return if we accept the channel request
     */
    bool onFileChannelRequest(const std::string& conversationId,
                              const std::string& member,
                              const std::string& fileId,
                              bool verifyShaSum = true) const;

    std::shared_ptr<TransferManager> dataTransfer(const std::string& id) const;

    /**
     * Ask conversation's members to send back a previous transfer to this deviec
     * @param conversationId    Related conversation
     * @param interactionId     Related interaction
     * @param fileId            Related fileId
     * @param path              where to download the file
     */
    bool downloadFile(const std::string& conversationId,
                      const std::string& interactionId,
                      const std::string& fileId,
                      const std::string& path,
                      size_t start = 0,
                      size_t end = 0);

    // Conversation's infos management
    void updateConversationInfos(const std::string& conversationId,
                                 const std::map<std::string, std::string>& infos,
                                 bool sync = true);
    std::map<std::string, std::string> conversationInfos(const std::string& conversationId) const;
    std::vector<uint8_t> conversationVCard(const std::string& conversationId) const;

    void addConversationMember(const std::string& conversationId,
                               const std::string& contactUri,
                               bool sendRequest = true);
    void removeConversationMember(const std::string& conversationId,
                                  const std::string& contactUri,
                                  bool isDevice = false);
    std::vector<std::map<std::string, std::string>> getConversationMembers(
        const std::string& conversationId) const;

    // Message send/load
    void sendMessage(const std::string& conversationId,
                     const Json::Value& value,
                     const std::string& parent = "",
                     bool announce = true,
                     const OnDoneCb& cb = {});

    void sendMessage(const std::string& conversationId,
                     const std::string& message,
                     const std::string& parent = "",
                     const std::string& type = "text/plain",
                     bool announce = true,
                     const OnDoneCb& cb = {});
    // Received that a peer displayed a message
    void onMessageDisplayed(const std::string& peer,
                            const std::string& conversationId,
                            const std::string& interactionId);

    /**
     * Retrieve how many interactions there is from HEAD to interactionId
     * @param convId
     * @param interactionId     "" for getting the whole history
     * @return number of interactions since interactionId
     */
    uint32_t countInteractions(const std::string& convId,
                               const std::string& toId,
                               const std::string& fromId) const;

    void onSyncData(const SyncMsg& msg, const std::string& peerId, const std::string& deviceId);

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
    void acceptConversationRequest(const std::string& conversationId);
    void rmConversationRequest(const std::string& id);

    bool isConversation(const std::string& convId) const
    {
        std::lock_guard<std::mutex> lk(conversationsMtx_);
        return conversations_.find(convId) != conversations_.end();
    }

    /**
     * Add to the related conversation the call history message
     * @param uri           Peer number
     * @param duration_ms   The call duration in ms
     */
    void addCallHistoryMessage(const std::string& uri, uint64_t duration_ms);

    // Conversations
    // TODO private

    /**
     * Get related conversation with member
     * @param uri       The member to search for
     * @return the conversation id if found else empty
     */
    std::string getOneToOneConversation(const std::string& uri) const;
    std::shared_ptr<RepeatedTask> conversationsEventHandler {};
    void checkConversationsEvents();
    bool handlePendingConversations();
    std::mutex pendingConversationsFetchMtx_ {};
    std::map<std::string, PendingConversationFetch> pendingConversationsFetch_;

private:
    std::weak_ptr<ConversationModule> weak()
    {
        return std::static_pointer_cast<ConversationModule>(shared_from_this());
    }

    /**
     * Send a message notification to all members
     * @param conversation
     * @param commit
     * @param sync      If we send an update to other account's devices
     */
    void sendMessageNotification(const Conversation& conversation,
                                 const std::string& commitId,
                                 bool sync);
    void sendMessageNotification(const std::string& conversationId,
                                 const std::string& commitId,
                                 bool sync);

    // The following informations are stored on the disk
    mutable std::mutex convInfosMtx_;
    std::map<std::string, ConvInfo> convInfos_;
    mutable std::mutex conversationsRequestsMtx_;
    std::map<std::string, ConversationRequest> conversationsRequests_;

    /** Conversations */
    mutable std::mutex conversationsMtx_ {};
    std::map<std::string, std::shared_ptr<Conversation>> conversations_;

    std::weak_ptr<JamiAccount> account_;
    std::function<void()> needsSyncingCb_;
    std::function<void(const std::string&, std::map<std::string, std::string>&&)> sendMsgCb_;
    OnNeedGitCb onNeedGitSocket_;
    std::string accountId_ {};
    std::string deviceId_ {};
    std::string username_ {};
};

} // namespace jami