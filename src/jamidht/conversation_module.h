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

using ChannelCb = std::function<bool(const std::shared_ptr<ChannelSocket>&)>;
using NeedsGitCb = std::function<void(const std::string&, const std::string&, ChannelCb&&)>;
using SengMsgCb = std::function<void(const std::string&, std::map<std::string, std::string>&&)>;
using NeedsSyncingCb = std::function<void()>;

class ConversationModule
{
public:
    ConversationModule(std::weak_ptr<JamiAccount>&& account,
                       NeedsSyncingCb&& needsSyncingCb,
                       SengMsgCb&& sendMsgCb,
                       NeedsGitCb&& onNeedGitSocket);
    ~ConversationModule() = default;

    /**
     * Refresh informations about conversations
     */
    void loadConversations();

    /**
     * Return all conversation's id (including syncing ones)
     */
    std::vector<std::string> getConversations() const;

    /**
     * Get related conversation with member
     * @param uri       The member to search for
     * @return the conversation id if found else empty
     */
    std::string getOneToOneConversation(const std::string& uri) const;

    /**
     * Return conversation's requests
     */
    std::vector<std::map<std::string, std::string>> getConversationRequests() const;

    /**
     * Called when detecting a new trust request with linked one to one
     * @param uri               Sender's URI
     * @param conversationId    Related conversation's id
     * @param payload           VCard
     * @param received          Received time
     */
    void onTrustRequest(const std::string& uri,
                        const std::string& conversationId,
                        const std::vector<uint8_t>& payload,
                        time_t received);

    /**
     * Called when receiving a new conversation's request
     * @param from      sender
     * @param value     conversation's request
     */
    void onConversationRequest(const std::string& from, const Json::Value& value);

    /**
     * Called when a peer needs an invite for a conversation (generally after that they received
     * a commit notification for a conversation they don't have yet)
     * @param from
     * @param conversationId
     */
    void onNeedConversationRequest(const std::string& from, const std::string& conversationId);

    /**
     * Accepts a conversation's request
     * @param convId
     */
    void acceptConversationRequest(const std::string& conversationId);

    /**
     * Decline a conversation's request
     * @param convId
     */
    void declineConversationRequest(const std::string& conversationId);

    /**
     * Starts a new conversation
     * @param mode          Wanted mode
     * @param otherMember   If needed (one to one)
     * @return conversation's id
     */
    std::string startConversation(ConversationMode mode = ConversationMode::INVITES_ONLY,
                                  const std::string& otherMember = "");

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
     * Add to the related conversation the call history message
     * @param uri           Peer number
     * @param duration_ms   The call duration in ms
     */
    void addCallHistoryMessage(const std::string& uri, uint64_t duration_ms);

    // Received that a peer displayed a message
    void onMessageDisplayed(const std::string& peer,
                            const std::string& conversationId,
                            const std::string& interactionId);

    /**
     * Load conversation's messages
     * @param conversationId    conversation to load
     * @param fromMessage
     * @param n                 max interactions to load
     * @return id of the operation
     */
    uint32_t loadConversationMessages(const std::string& conversationId,
                                      const std::string& fromMessage = "",
                                      size_t n = 0);

    // File transfer
    /**
     * Returns related transfer manager
     * @param id        conversation's id
     * @return nullptr if not found, else the manager
     */
    std::shared_ptr<TransferManager> dataTransfer(const std::string& id) const;

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

    // Sync
    /**
     * Sync conversations with detected peer
     */
    void syncConversations(const std::string& peer, const std::string& deviceId);

    /**
     * Detects new conversations and requests from other devices
     * @param msg       Received data
     * @param peerId    Sender
     * @param deviceId
     */
    void onSyncData(const SyncMsg& msg, const std::string& peerId, const std::string& deviceId);

    /**
     * Check if we needs to share infos with a contact
     * @param memberUri
     * @param deviceId
     */
    bool needsSyncingWith(const std::string& memberUri, const std::string& deviceId) const;

    /**
     * Inform module that a peer fetched a commit
     * @note: this definitely remove the repository when needed
     * @param conversationId    related conv
     * @param deviceId          device who synced
     */
    void setFetched(const std::string& conversationId, const std::string& deviceId);

    /**
     * Launch fetch on new commit
     * @param peer              Who sent the notification
     * @param deviceId          Who sent the notification
     * @param conversationId    Related conversation
     * @param commitId          Commit to retrieve
     */
    void onNewGitCommit(const std::string& peer,
                        const std::string& deviceId,
                        const std::string& conversationId,
                        const std::string& commitId);

    // Conversation's member
    /**
     * Adds a new member to a conversation (this will triggers a member event + new message on success)
     * @param conversationId
     * @param contactUri
     * @param sendRequest   If we need to inform the peer (used for tests)
     */
    void addConversationMember(const std::string& conversationId,
                               const std::string& contactUri,
                               bool sendRequest = true);
    /**
     * Removes a member from a conversation (this will triggers a member event + new message on success)
     * @param conversationId
     * @param contactUri
     * @param isDevice
     */
    void removeConversationMember(const std::string& conversationId,
                                  const std::string& contactUri,
                                  bool isDevice = false);
    /**
     * Get members
     * @param conversationId
     * @return a map of members with their role and details
     */
    std::vector<std::map<std::string, std::string>> getConversationMembers(
        const std::string& conversationId) const;
    /**
     * Retrieve how many interactions there is from HEAD to interactionId
     * @param convId
     * @param interactionId     "" for getting the whole history
     * @return number of interactions since interactionId
     */
    uint32_t countInteractions(const std::string& convId,
                               const std::string& toId,
                               const std::string& fromId) const;

    // Conversation's infos management
    /**
     * Update metadatas from conversations (like title, avatar, etc)
     * @param conversationId
     * @param infos
     * @param sync              If we need to sync with others (used for tests)
     */
    void updateConversationInfos(const std::string& conversationId,
                                 const std::map<std::string, std::string>& infos,
                                 bool sync = true);
    std::map<std::string, std::string> conversationInfos(const std::string& conversationId) const;
    // Get the map into a VCard format for storing
    std::vector<uint8_t> conversationVCard(const std::string& conversationId) const;

    /**
     * Return if a device is banned of a conversation
     * @param convId
     * @param deviceId
     */
    bool isBannedDevice(const std::string& convId, const std::string& deviceId) const;

    // Remove swarm
    /**
     * Remove one to one conversations related to a contact
     * @param uri       Of the contact
     * @param ban       If banned
     */
    void removeContact(const std::string& uri, bool ban);

    /**
     * Remove a conversation, but not the contact
     * @param conversationId
     * @return if successfully removed
     */
    bool removeConversation(const std::string& conversationId);

    /**
     * When a DHT message is coming, during swarm transition
     * check if a swarm is linked to that contact and removes
     * the swarm if needed
     * @param peerUri   the one who sent a DHT message
     */
    void checkIfRemoveForCompat(const std::string& peerUri);

    // The following methods modify what is stored on the disk
    static void saveConvInfos(const std::string& accountId,
                              const std::map<std::string, ConvInfo>& conversations);
    static void saveConvRequests(
        const std::string& accountId,
        const std::map<std::string, ConversationRequest>& conversationsRequests);

    static std::map<std::string, ConvInfo> convInfos(const std::string& accountId);
    static std::map<std::string, ConversationRequest> convRequests(const std::string& accountId);
    void addConvInfo(const ConvInfo& info);
    void setConversationMembers(const std::string& convId, const std::vector<std::string>& members);

private:
    class Impl;
    std::shared_ptr<Impl> pimpl_;
};

} // namespace jami