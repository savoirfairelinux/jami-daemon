/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#pragma once

#include "jamidht/conversationrepository.h"
#include "conversationrepository.h"
#include "swarm/swarm_protocol.h"
#include "jami/conversation_interface.h"
#include "jamidht/typers.h"

#include <json/json.h>
#include <msgpack.hpp>

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>

#include <asio.hpp>

namespace dhtnet {
class ChannelSocket;
} // namespace dhtnet

namespace jami {

namespace ConversationMapKeys {
static constexpr const char* ID = "id";
static constexpr const char* CREATED = "created";
static constexpr const char* REMOVED = "removed";
static constexpr const char* ERASED = "erased";
static constexpr const char* MEMBERS = "members";
static constexpr const char* LAST_DISPLAYED = "lastDisplayed";
static constexpr const char* PREFERENCES = "preferences";
static constexpr const char* ACTIVE_CALLS = "activeCalls";
static constexpr const char* HOSTED_CALLS = "hostedCalls";
static constexpr const char* CACHED = "cached";
static constexpr const char* RECEIVED = "received";
static constexpr const char* DECLINED = "declined";
static constexpr const char* FROM = "from";
static constexpr const char* CONVERSATIONID = "conversationId";
static constexpr const char* METADATAS = "metadatas";
} // namespace ConversationMapKeys

namespace ConversationPreferences {
static constexpr const char* HOST_CONFERENCES = "hostConferences";
}

/**
 * A ConversationRequest is a request which corresponds to a trust request, but for conversations
 * It's signed by the sender and contains the members list, the conversationId, and the metadatas
 * such as the conversation's vcard, etc. (TODO determine)
 * Transmitted via the UDP DHT
 */
struct ConversationRequest
{
    std::string conversationId;
    std::string from;
    std::map<std::string, std::string> metadatas;

    time_t received {0};
    time_t declined {0};

    ConversationRequest() = default;
    ConversationRequest(const Json::Value& json);

    Json::Value toJson() const;
    std::map<std::string, std::string> toMap() const;

    bool operator==(const ConversationRequest& o) const
    {
        auto m = toMap();
        auto om = o.toMap();
        return m.size() == om.size() && std::equal(m.begin(), m.end(), om.begin());
    }

    bool isOneToOne() const {
        try {
            return metadatas.at("mode") == "0";
        } catch (...) {}
        return true;
    }

    MSGPACK_DEFINE_MAP(from, conversationId, metadatas, received, declined)
};

struct ConvInfo
{
    std::string id {};
    time_t created {0};
    time_t removed {0};
    time_t erased {0};
    std::set<std::string> members;
    std::string lastDisplayed {};

    ConvInfo() = default;
    ConvInfo(const ConvInfo&) = default;
    ConvInfo(ConvInfo&&) = default;
    ConvInfo(const std::string& id) : id(id) {};
    explicit ConvInfo(const Json::Value& json);

    bool isRemoved() const { return removed >= created; }

    ConvInfo& operator=(const ConvInfo&) = default;
    ConvInfo& operator=(ConvInfo&&) = default;

    Json::Value toJson() const;

    MSGPACK_DEFINE_MAP(id, created, removed, erased, members, lastDisplayed)
};

class JamiAccount;
class ConversationRepository;
class TransferManager;
enum class ConversationMode;

using OnPullCb = std::function<void(bool fetchOk)>;
using OnLoadMessages
    = std::function<void(std::vector<std::map<std::string, std::string>>&& messages)>;
using OnLoadMessages2
    = std::function<void(std::vector<libjami::SwarmMessage>&& messages)>;
using OnCommitCb = std::function<void(const std::string&)>;
using OnDoneCb = std::function<void(bool, const std::string&)>;
using OnMultiDoneCb = std::function<void(const std::vector<std::string>&)>;
using OnMembersChanged = std::function<void(const std::set<std::string>&)>;
using DeviceId = dht::PkId;
using GitSocketList = std::map<DeviceId, std::shared_ptr<dhtnet::ChannelSocket>>;
using ChannelCb = std::function<bool(const std::shared_ptr<dhtnet::ChannelSocket>&)>;
using NeedSocketCb
    = std::function<void(const std::string&, const std::string&, ChannelCb&&, const std::string&)>;

class Conversation : public std::enable_shared_from_this<Conversation>
{
public:
    Conversation(const std::shared_ptr<JamiAccount>& account,
                 ConversationMode mode,
                 const std::string& otherMember = "");
    Conversation(const std::shared_ptr<JamiAccount>& account,
                 const std::string& conversationId = "");
    Conversation(const std::shared_ptr<JamiAccount>& account,
                 const std::string& remoteDevice,
                 const std::string& conversationId);
    ~Conversation();

    /**
     * Print the state of the DRT linked to the conversation
     */
    void monitor();

#ifdef LIBJAMI_TESTABLE
    enum class BootstrapStatus { FAILED, FALLBACK, SUCCESS };
    /**
     * Used by the tests to get whenever the DRT is connected/disconnected
     */
    void onBootstrapStatus(const std::function<void(std::string, BootstrapStatus)>& cb);
#endif

    /**
     * Bootstrap swarm manager to other peers
     * @param onBootstraped     Callback called when connection is successfully established
     * @param knownDevices      List of account's known devices
     */
    void bootstrap(std::function<void()> onBootstraped, const std::vector<DeviceId>& knownDevices);

    /**
     * Refresh active calls.
     * @note: If the host crash during a call, when initializing, we need to update
     * and commit all the crashed calls
     * @return  Commits added
     */
    std::vector<std::string> commitsEndedCalls();

    void onMembersChanged(OnMembersChanged&& cb);

    /**
     * Set the callback that will be called whenever a new socket will be needed
     * @param cb
     */
    void onNeedSocket(NeedSocketCb cb);
    /**
     * Add swarm connection to the DRT
     * @param channel       Related channel
     */
    void addSwarmChannel(std::shared_ptr<dhtnet::ChannelSocket> channel);

    /**
     * Get conversation's id
     * @return conversation Id
     */
    std::string id() const;

    // Member management
    /**
     * Add conversation member
     * @param uri   Member to add
     * @param cb    On done cb
     */
    void addMember(const std::string& contactUri, const OnDoneCb& cb = {});
    void removeMember(const std::string& contactUri, bool isDevice, const OnDoneCb& cb = {});
    /**
     * @param includeInvited        If we want invited members
     * @param includeLeft           If we want left members
     * @param includeBanned         If we want banned members
     * @return a vector of member details:
     * {
     *  "uri":"xxx",
     *  "role":"member/admin/invited",
     *  "lastDisplayed":"id"
     *  ...
     * }
     */
    std::vector<std::map<std::string, std::string>> getMembers(bool includeInvited = false,
                                                               bool includeLeft = false,
                                                               bool includeBanned = false) const;

    /**
     * @param filter           If we want to remove one member
     * @param filteredRoles    If we want to ignore some roles
     * @return members' uris
     */
    std::set<std::string> memberUris(
        std::string_view filter = {},
        const std::set<MemberRole>& filteredRoles = {MemberRole::INVITED,
                                                     MemberRole::LEFT,
                                                     MemberRole::BANNED}) const;

    /**
     * Get peers to sync with. This is mostly managed by the DRT
     * @return some mobile nodes and all connected nodes
     */
    std::vector<NodeId> peersToSyncWith() const;
    /**
     * Check if we're at least connected to one node
     * @return if the DRT is connected
     */
    bool isBootstraped() const;
    /**
     * Retrieve the uri from a deviceId
     * @note used by swarm manager (peersToSyncWith)
     * @param deviceId
     * @return corresponding issuer
     */
    std::string uriFromDevice(const std::string& deviceId) const;

    /**
     * Join a conversation
     * @return commit id to send
     */
    std::string join();

    /**
     * Test if an URI is a member
     * @param uri       URI to test
     * @return true if uri is a member
     */
    bool isMember(const std::string& uri, bool includeInvited = false) const;
    bool isBanned(const std::string& uri) const;

    // Message send
    void sendMessage(std::string&& message,
                     const std::string& type = "text/plain",
                     const std::string& replyTo = "",
                     OnCommitCb&& onCommit = {},
                     OnDoneCb&& cb = {});
    void sendMessage(Json::Value&& message,
                     const std::string& replyTo = "",
                     OnCommitCb&& onCommit = {},
                     OnDoneCb&& cb = {});
    // Note: used for replay. Should not be used by clients
    void sendMessages(std::vector<Json::Value>&& messages, OnMultiDoneCb&& cb = {});
    /**
     * Get a range of messages
     * @param cb        The callback when loaded
     * @param options   The log options
     */
    void loadMessages(OnLoadMessages cb, const LogOptions& options);
    /**
     * Get a range of messages
     * @param cb        The callback when loaded
     * @param options   The log options
     */
    void loadMessages2(const OnLoadMessages2& cb, const LogOptions& options);
    /**
     * Clear all cached messages
     */
    void clearCache();
    /**
     * Retrieve one commit
     * @param   commitId
     * @return  The commit if found
     */
    std::optional<std::map<std::string, std::string>> getCommit(const std::string& commitId) const;
    /**
     * Get last commit id
     * @return last commit id
     */
    std::string lastCommitId() const;

    /**
     * Fetch and merge from peer
     * @param deviceId  Peer device
     * @param cb        On pulled callback
     * @param commitId  Commit id that triggered this fetch
     * @return true if callback will be called later
     */
    bool pull(const std::string& deviceId, OnPullCb&& cb, std::string commitId = "");
    /**
     * Fetch new commits and re-ask for waiting files
     * @param member
     * @param deviceId
     * @param cb        cf pull()
     * @param commitId  cf pull()
     */
    void sync(const std::string& member,
              const std::string& deviceId,
              OnPullCb&& cb,
              std::string commitId = "");

    /**
     * Generate an invitation to send to new contacts
     * @return the invite to send
     */
    std::map<std::string, std::string> generateInvitation() const;

    /**
     * Leave a conversation
     * @return commit id to send
     */
    std::string leave();

    /**
     * Set a conversation as removing (when loading convInfo and still not sync)
     * @todo: not a big fan to see this here. can be set in the constructor
     * cause it's used by jamiaccount when loading conversations
     */
    void setRemovingFlag();

    /**
     * Check if we are removing the conversation
     * @return true if left the room
     */
    bool isRemoving();

    /**
     * Erase all related datas
     */
    void erase();

    /**
     * Get conversation's mode
     * @return the mode
     */
    ConversationMode mode() const;

    /**
     * One to one util, get initial members
     * @return initial members
     */
    std::vector<std::string> getInitialMembers() const;
    bool isInitialMember(const std::string& uri) const;

    /**
     * Change repository's infos
     * @param map       New infos (supported keys: title, description, avatar)
     * @param cb        On commited
     */
    void updateInfos(const std::map<std::string, std::string>& map, const OnDoneCb& cb = {});

    /**
     * Change user's preferences
     * @param map       New preferences
     */
    void updatePreferences(const std::map<std::string, std::string>& map);

    /**
     * Retrieve current infos (title, description, avatar, mode)
     * @return infos
     */
    std::map<std::string, std::string> infos() const;
    /**
     * Retrieve current preferences (color, notification, etc)
     * @param includeLastModified       If we want to know when the preferences were modified
     * @return preferences
     */
    std::map<std::string, std::string> preferences(bool includeLastModified) const;
    std::vector<uint8_t> vCard() const;

    /////// File transfer

    /**
     * Access to transfer manager
     */
    std::shared_ptr<TransferManager> dataTransfer() const;

    /**
     * Choose if we can accept channel request
     * @param member        member to check
     * @param fileId        file transfer to check (needs to be waiting)
     * @param verifyShaSum  for debug only
     * @return if we accept the channel request
     */
    bool onFileChannelRequest(const std::string& member,
                              const std::string& fileId,
                              bool verifyShaSum = true) const;
    /**
     * Adds a file to the waiting list and ask members
     * @param interactionId     Related interaction id
     * @param fileId            Related id
     * @param path              Destination
     * @param member            Member if we know from who to pull file
     * @param deviceId          Device if we know from who to pull file
     * @param start             Offset (unused for now)
     * @param end               Offset (unused)
     * @return id of the file
     */
    bool downloadFile(const std::string& interactionId,
                      const std::string& fileId,
                      const std::string& path,
                      const std::string& member = "",
                      const std::string& deviceId = "",
                      std::size_t start = 0,
                      std::size_t end = 0);

    /**
     * Reset fetched information
     */
    void clearFetched();
    /**
     * Store information about who fetch or not. This simplify sync (sync when a device without the
     * last fetch is detected)
     * @param deviceId
     * @param commitId
     */
    void hasFetched(const std::string& deviceId, const std::string& commitId);

    /**
     * Store last read commit (returned in getMembers)
     * @param uri               Of the member
     * @param interactionId     Last interaction displayed
     * @return if updated
     */
    bool setMessageDisplayed(const std::string& uri, const std::string& interactionId);
    /**
     * Retrieve last displayed and fetch status per member
     * @return A map with the following structure:
     * {uri, {
     *          {"fetch", "commitId"},
     *          {"fetched_ts", "timestamp"},
     *          {"read", "commitId"},
     *          {"read_ts", "timestamp"}
     *       }
     * }
     */
    std::map<std::string, std::map<std::string, std::string>> messageStatus() const;
    /**
     * Update fetch/read status
     * @param messageStatus     A map with the following structure:
     * {uri, {
     *          {"fetch", "commitId"},
     *          {"fetched_ts", "timestamp"},
     *          {"read", "commitId"},
     *          {"read_ts", "timestamp"}
     *       }
     * }
     */
    void updateMessageStatus(const std::map<std::string, std::map<std::string, std::string>>& messageStatus);
    void onMessageStatusChanged(const std::function<void(const std::map<std::string, std::map<std::string, std::string>>&)>& cb);
    /**
     * Retrieve how many interactions there is from HEAD to interactionId
     * @param toId      "" for getting the whole history
     * @param fromId    "" => HEAD
     * @param authorURI author to stop counting
     * @return number of interactions since interactionId
     */
    uint32_t countInteractions(const std::string& toId,
                               const std::string& fromId = "",
                               const std::string& authorUri = "") const;
    /**
     * Search in the conversation via a filter
     * @param req       Id of the request
     * @param filter    Parameters for the search
     * @param flag      To check when search is finished
     * @note triggers messagesFound
     */
    void search(uint32_t req,
                const Filter& filter,
                const std::shared_ptr<std::atomic_int>& flag) const;
    /**
     * Host a conference in the conversation
     * @note the message must have "confId"
     * @note Update hostedCalls_ and commit in the conversation
     * @param message       message to commit
     * @param cb            callback triggered when committed
     */
    void hostConference(Json::Value&& message, OnDoneCb&& cb = {});
    /**
     * Announce the end of a call
     * @note the message must have "confId"
     * @note called when conference is finished
     * @param message       message to commit
     * @param cb            callback triggered when committed
     */
    void removeActiveConference(Json::Value&& message, OnDoneCb&& cb = {});
    /**
     * Check if we're currently hosting this conference
     * @param confId
     * @return true if hosting
     */
    bool isHosting(const std::string& confId) const;
    /**
     * Return current detected calls
     * @return a vector of map with the following keys: "id", "uri", "device"
     */
    std::vector<std::map<std::string, std::string>> currentCalls() const;

    /**
     * Git operations will need a ChannelSocket for cloning/fetching commits
     * Because libgit2 is a C library, we store the pointer in the corresponding conversation
     * and the GitTransport will inject to libgit2 whenever needed
     */
    std::shared_ptr<dhtnet::ChannelSocket> gitSocket(const DeviceId& deviceId) const;
    void addGitSocket(const DeviceId& deviceId, const std::shared_ptr<dhtnet::ChannelSocket>& socket);
    void removeGitSocket(const DeviceId& deviceId);

    /**
     * Stop SwarmManager, bootstrap and gitSockets
     */
    void shutdownConnections();
    /**
     * Used to avoid multiple connections, we just check if we got a swarm channel with a specific
     * device
     * @param deviceId
     */
    bool hasSwarmChannel(const std::string& deviceId);

    /**
     * If we change from one network to one another, we will need to update the state of the connections
     */
    void connectivityChanged();

    /**
     * @return getAllNodes()    Nodes that are linked to the conversation
    */
    std::vector<jami::DeviceId> getDeviceIdList() const;

    /**
     * Get Typers object
     * @return Typers object
     */
    std::shared_ptr<Typers> typers() const;

private:
    std::shared_ptr<Conversation> shared()
    {
        return std::static_pointer_cast<Conversation>(shared_from_this());
    }
    std::shared_ptr<Conversation const> shared() const
    {
        return std::static_pointer_cast<Conversation const>(shared_from_this());
    }
    std::weak_ptr<Conversation> weak()
    {
        return std::static_pointer_cast<Conversation>(shared_from_this());
    }
    std::weak_ptr<Conversation const> weak() const
    {
        return std::static_pointer_cast<Conversation const>(shared_from_this());
    }

    // Private because of weak()
    /**
     * Used by bootstrap() to launch the fallback
     * @param ec
     * @param members       Members to try to connect
     */
    void checkBootstrapMember(const asio::error_code& ec,
                              std::vector<std::map<std::string, std::string>> members);

    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
