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

class ConversationModule :  public std::enable_shared_from_this<ConversationModule> // TODO Module interface
{
public:
    ConversationModule(
        std::weak_ptr<JamiAccount>&& account_,
        std::function<void()>&& needsSyncingCb,
        std::function<void(std::string&&, std::map<std::string, std::string>&&)>&& sendMsgCb);
    ~ConversationModule() = default;

    std::vector<std::string> getConversations() const;
    std::vector<std::map<std::string, std::string>> getConversationRequests() const;

    void declineConversationRequest(const std::string& conversationId);

    bool removeConversation(const std::string& conversationId);
    /**
     * Remove a repository and all files
     * @param convId
     * @param sync      If we send an update to other account's devices
     * @param force     True if ignore the removing flag
     */
    void removeRepository(const std::string& convId, bool sync, bool force = false);

    // TODO load
    // TODO needsSyncing, sendDhtMessage, sendMessageNotification

    // TODO remove accountManager::conversations/conversationsRequests

    void checkIfRemoveForCompat(const std::string& peerUri);

    // Invites
    void onConversationRequest(const std::string& from, const Json::Value&);
    void onNeedConversationRequest(const std::string& from,
                                   const std::string& conversationId);

    std::string startConversation(ConversationMode mode = ConversationMode::INVITES_ONLY,
                                  const std::string& otherMember = "");

    uint32_t loadConversationMessages(const std::string& conversationId,
                                      const std::string& fromMessage = "",
                                      size_t n = 0);

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

    /**
     * Retrieve how many interactions there is from HEAD to interactionId
     * @param convId
     * @param interactionId     "" for getting the whole history
     * @return number of interactions since interactionId
     */
    uint32_t countInteractions(const std::string& convId,
                               const std::string& toId,
                               const std::string& fromId) const;

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
    std::function<void(std::string&&, std::map<std::string, std::string>&&)> sendMsgCb_;
    std::string accountId_ {};
    std::string deviceId_ {};
    std::string username_ {};
};

} // namespace jami