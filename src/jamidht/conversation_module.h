/*
 *  Copyright (C) 2021-2024 Savoir-faire Linux Inc.
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
#include "jamidht/account_manager.h"
#include "jamidht/conversation.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/jami_contact.h"

#include <mutex>
#include <msgpack.hpp>

namespace jami {
static constexpr const char MIME_TYPE_INVITE[] {"application/invite"};
static constexpr const char MIME_TYPE_GIT[] {"application/im-gitmessage-id"};

class SIPCall;

struct SyncMsg
{
    DeviceSync ds;
    std::map<std::string, ConvInfo> c;
    std::map<std::string, ConversationRequest> cr;
    // p is conversation's preferences. It's not stored in c, as
    // we can update the preferences without touching any confInfo.
    std::map<std::string, std::map<std::string, std::string>> p;
    // Last displayed messages [[deprecated]]
    std::map<std::string, std::map<std::string, std::string>> ld;
    // Read & fetched status
    /*
     * {{ convId,
     *      { memberUri,, {
     *          {"fetch", "commitId"},
     *          {"fetched_ts", "timestamp"},
     *          {"read", "commitId"},
     *          {"read_ts", "timestamp"}
     *       }
     * }}
     */
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> ms;
    MSGPACK_DEFINE(ds, c, cr, p, ld, ms)
};

using ChannelCb = std::function<bool(const std::shared_ptr<dhtnet::ChannelSocket>&)>;
using NeedSocketCb
    = std::function<void(const std::string&, const std::string&, ChannelCb&&, const std::string&)>;
using SengMsgCb = std::function<
    uint64_t(const std::string&, const DeviceId&, std::map<std::string, std::string>, uint64_t)>;
using NeedsSyncingCb = std::function<void(std::shared_ptr<SyncMsg>&&)>;
using OneToOneRecvCb = std::function<void(const std::string&, const std::string&)>;

class ConversationModule
{
public:
    ConversationModule(std::shared_ptr<JamiAccount> account,
                       std::shared_ptr<AccountManager> accountManager,
                       NeedsSyncingCb&& needsSyncingCb,
                       SengMsgCb&& sendMsgCb,
                       NeedSocketCb&& onNeedSocket,
                       NeedSocketCb&& onNeedSwarmSocket,
                       OneToOneRecvCb&& oneToOneRecvCb,
                       bool autoLoadConversations = true);
    ~ConversationModule() = default;

    void setAccountManager(std::shared_ptr<AccountManager> accountManager);

    /**
     * Refresh information about conversations
     */
    void loadConversations();

    void loadSingleConversation(const std::string& convId);

#ifdef LIBJAMI_TESTABLE
    void onBootstrapStatus(const std::function<void(std::string, Conversation::BootstrapStatus)>& cb);
#endif

    void monitor();

    /**
     * Bootstrap swarm managers to other peers
     */
    void bootstrap(const std::string& convId = "");

    /**
     * Clear not removed fetch
     */
    void clearPendingFetch();

    /**
     * Reload requests from file
     */
    void reloadRequests();

    /**
     * Return all conversation's id (including syncing ones)
     */
    std::vector<std::string> getConversations() const;

    /**
     * Get related conversation with member
     * @param uri       The member to search for
     * @return the conversation id if found else empty
     */
    std::string getOneToOneConversation(const std::string& uri) const noexcept;

    /**
     * Replace linked conversation in contact's details
     * @param uri           Of the contact
     * @param oldConv       Current conversation
     * @param newConv
     * @return if replaced
     */
    bool updateConvForContact(const std::string& uri,
                              const std::string& oldConv,
                              const std::string& newConv);

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
     * @param from      Sender
     * @param value     Conversation's request
     */
    void onConversationRequest(const std::string& from, const Json::Value& value);

    /**
     * Retrieve author of a conversation request
     * @param convId    Conversation's id
     * @return the author of the conversation request
     */
    std::string peerFromConversationRequest(const std::string& convId) const;

    /**
     * Called when a peer needs an invite for a conversation (generally after that they received
     * a commit notification for a conversation they don't have yet)
     * @param from
     * @param conversationId
     */
    void onNeedConversationRequest(const std::string& from, const std::string& conversationId);

    /**
     * Accept a conversation's request
     * @param convId
     * @param deviceId      If a trust request is accepted from a device (can help to sync)
     */
    void acceptConversationRequest(const std::string& conversationId,
                                   const std::string& deviceId = "");

    /**
     * Decline a conversation's request
     * @param convId
     */
    void declineConversationRequest(const std::string& conversationId);

    /**
     * Clone conversation from a member
     * @note used to clone an old conversation after deleting/re-adding a contact
     * @param conversationId
     * @param uri
     * @param oldConvId
     */
    void cloneConversationFrom(const std::string& conversationId,
                               const std::string& uri,
                               const std::string& oldConvId = "");

    /**
     * Starts a new conversation
     * @param mode          Wanted mode
     * @param otherMember   If needed (one to one)
     * @return conversation's id
     */
    std::string startConversation(ConversationMode mode = ConversationMode::INVITES_ONLY,
                                  const dht::InfoHash& otherMember = {});

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
    void reactToMessage(const std::string& conversationId,
                        const std::string& newBody,
                        const std::string& reactToId);

    /**
     * Add to the related conversation the call history message
     * @param uri           Peer number
     * @param duration_ms   The call duration in ms
     * @param reason
     */
    void addCallHistoryMessage(const std::string& uri, uint64_t duration_ms, const std::string& reason);

    // Received that a peer displayed a message
    bool onMessageDisplayed(const std::string& peer,
                            const std::string& conversationId,
                            const std::string& interactionId);
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> convMessageStatus() const;

    /**
     * Load conversation's messages
     * @param conversationId    Conversation to load
     * @param fromMessage
     * @param n                 Max interactions to load
     * @return id of the operation
     */
    uint32_t loadConversationMessages(const std::string& conversationId,
                                      const std::string& fromMessage = "",
                                      size_t n = 0);
    uint32_t loadConversation(const std::string& conversationId,
                              const std::string& fromMessage = "",
                              size_t n = 0);
    uint32_t loadConversationUntil(const std::string& conversationId,
                                   const std::string& fromMessage,
                                   const std::string& to);
    uint32_t loadSwarmUntil(const std::string& conversationId,
                            const std::string& fromMessage,
                            const std::string& toMessage);
    /**
     * Clear loaded interactions
     * @param conversationId
     */
    void clearCache(const std::string& conversationId);

    // File transfer
    /**
     * Returns related transfer manager
     * @param id        Conversation's id
     * @return nullptr if not found, else the manager
     */
    std::shared_ptr<TransferManager> dataTransfer(const std::string& id) const;

    /**
     * Choose if we can accept channel request
     * @param member        Member to check
     * @param fileId        File transfer to check (needs to be waiting)
     * @param verifyShaSum  For debug only
     * @return if we accept the channel request
     */
    bool onFileChannelRequest(const std::string& conversationId,
                              const std::string& member,
                              const std::string& fileId,
                              bool verifyShaSum = true) const;

    /**
     * Ask conversation's members to send a file to this device
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
     * Detect new conversations and request from other devices
     * @param msg       Received data
     * @param peerId    Sender
     * @param deviceId
     */
    void onSyncData(const SyncMsg& msg, const std::string& peerId, const std::string& deviceId);

    /**
     * Check if we need to share infos with a contact
     * @param memberUri
     * @param deviceId
     */
    bool needsSyncingWith(const std::string& memberUri, const std::string& deviceId) const;

    /**
     * Notify that a peer fetched a commit
     * @note: this definitely remove the repository when needed (when we left and someone fetched
     * the information)
     * @param conversationId    Related conv
     * @param deviceId          Device who synced
     * @param commit            HEAD synced
     */
    void setFetched(const std::string& conversationId,
                    const std::string& deviceId,
                    const std::string& commit);

    /**
     * Launch fetch on new commit
     * @param peer              Who sent the notification
     * @param deviceId          Who sent the notification
     * @param conversationId    Related conversation
     * @param commitId          Commit to retrieve
     */
    void fetchNewCommits(const std::string& peer,
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
                               const dht::InfoHash& contactUri,
                               bool sendRequest = true);
    /**
     * Remove a member from a conversation (this will trigger a member event + new message on success)
     * @param conversationId
     * @param contactUri
     * @param isDevice
     */
    void removeConversationMember(const std::string& conversationId,
                                  const dht::InfoHash& contactUri,
                                  bool isDevice = false);
    /**
     * Get members
     * @param conversationId
     * @param includeBanned
     * @return a map of members with their role and details
     */
    std::vector<std::map<std::string, std::string>> getConversationMembers(
        const std::string& conversationId, bool includeBanned = false) const;
    /**
     * Retrieve the number of interactions from interactionId to HEAD
     * @param convId
     * @param interactionId     "" for getting the whole history
     * @param authorUri         Stop when detect author
     * @return number of interactions since interactionId
     */
    uint32_t countInteractions(const std::string& convId,
                               const std::string& toId,
                               const std::string& fromId,
                               const std::string& authorUri) const;

    /**
     * Search in conversations via a filter
     * @param req       Id of the request
     * @param convId    Leave empty to search in all conversation, else add the conversation's id
     * @param filter    Parameters for the search
     * @note triggers messagesFound
     */
    void search(uint32_t req, const std::string& convId, const Filter& filter) const;

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
    /**
     * Update user's preferences (like color, notifications, etc) to be synced across devices
     * @param conversationId
     * @param preferences
     */
    void setConversationPreferences(const std::string& conversationId,
                                    const std::map<std::string, std::string>& prefs);
    std::map<std::string, std::string> getConversationPreferences(const std::string& conversationId,
                                                                  bool includeCreated = false) const;
    /**
     * Retrieve all conversation preferences to sync with other devices
     */
    std::map<std::string, std::map<std::string, std::string>> convPreferences() const;
    // Get the map into a VCard format for storing
    std::vector<uint8_t> conversationVCard(const std::string& conversationId) const;

    /**
     * Return if a device or member is banned from a conversation
     * @param convId
     * @param uri
     */
    bool isBanned(const std::string& convId, const std::string& uri) const;

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
    void initReplay(const std::string& oldConvId, const std::string& newConvId);
    /**
     * Check if we're hosting a specific conference
     * @param conversationId (empty to search all conv)
     * @param confId
     * @return true if hosting this conference
     */
    bool isHosting(const std::string& conversationId, const std::string& confId) const;
    /**
     * Return active calls
     * @param convId        Which conversation to choose
     * @return {{"id":id}, {"uri":uri}, {"device":device}}
     */
    std::vector<std::map<std::string, std::string>> getActiveCalls(
        const std::string& conversationId) const;
    /**
     * Call the conversation
     * @param url       Url to call (swarm:conversation or swarm:conv/account/device/conf to join)
     * @param mediaList The media list
     * @param cb        Callback to pass which device to call (called in the same thread)
     * @return call if a call is started, else nullptr
     */
    std::shared_ptr<SIPCall> call(const std::string& url,
                                const std::vector<libjami::MediaMap>& mediaList,
                                std::function<void(const std::string&, const DeviceId&, const std::shared_ptr<SIPCall>&)>&& cb);
    void hostConference(const std::string& conversationId,
                        const std::string& confId,
                        const std::string& callId,
                        const std::vector<libjami::MediaMap>& mediaList = {});

    // The following methods modify what is stored on the disk
    static void saveConvInfos(const std::string& accountId,
                              const std::map<std::string, ConvInfo>& conversations);
    static void saveConvInfosToPath(const std::filesystem::path& path,
                                    const std::map<std::string, ConvInfo>& conversations);
    static void saveConvRequests(
        const std::string& accountId,
        const std::map<std::string, ConversationRequest>& conversationsRequests);
    static void saveConvRequestsToPath(
        const std::filesystem::path& path,
        const std::map<std::string, ConversationRequest>& conversationsRequests);

    static std::map<std::string, ConvInfo> convInfos(const std::string& accountId);
    static std::map<std::string, ConvInfo> convInfosFromPath(const std::filesystem::path& path);
    static std::map<std::string, ConversationRequest> convRequests(const std::string& accountId);
    static std::map<std::string, ConversationRequest> convRequestsFromPath(
        const std::filesystem::path& path);
    void addConvInfo(const ConvInfo& info);

    /**
     * Get a conversation
     * @param convId
     */
    std::shared_ptr<Conversation> getConversation(const std::string& convId);
    /**
     * Return current git socket used for a conversation
     * @param deviceId          Related device
     * @param conversationId    Related conversation
     * @return the related socket
     */
    std::shared_ptr<dhtnet::ChannelSocket> gitSocket(std::string_view deviceId,
                                                     std::string_view convId) const;
    void removeGitSocket(std::string_view deviceId, std::string_view convId);
    void addGitSocket(std::string_view deviceId,
                      std::string_view convId,
                      const std::shared_ptr<dhtnet::ChannelSocket>& channel);
    /**
     * Clear all connection (swarm channels)
     */
    void shutdownConnections();
    /**
     * Add a swarm connection
     * @param conversationId
     * @param socket
     */
    void addSwarmChannel(const std::string& conversationId,
                         std::shared_ptr<dhtnet::ChannelSocket> socket);
    /**
     * Triggers a bucket maintainance for DRTs
     */
    void connectivityChanged();

    /**
     * Get Typers object for a conversation
     * @param convId
     * @return the Typer object
     */
    std::shared_ptr<Typers> getTypers(const std::string& convId);

private:
    class Impl;
    std::shared_ptr<Impl> pimpl_;
};

} // namespace jami
