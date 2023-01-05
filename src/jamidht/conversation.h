/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "jamidht/conversationrepository.h"
#include "jami/datatransfer_interface.h"
#include "conversationrepository.h"

#include <json/json.h>
#include <msgpack.hpp>

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>

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

    MSGPACK_DEFINE_MAP(from, conversationId, metadatas, received, declined)
};

struct ConvInfo
{
    std::string id {};
    time_t created {0};
    time_t removed {0};
    time_t erased {0};
    std::vector<std::string> members;
    std::string lastDisplayed {};

    ConvInfo() = default;
    ConvInfo(const Json::Value& json);

    Json::Value toJson() const;

    MSGPACK_DEFINE_MAP(id, created, removed, erased, members, lastDisplayed)
};

class JamiAccount;
class ConversationRepository;
class TransferManager;
class ChannelSocket;
enum class ConversationMode;

using OnPullCb = std::function<void(bool fetchOk)>;
using OnLoadMessages
    = std::function<void(std::vector<std::map<std::string, std::string>>&& messages)>;
using OnCommitCb = std::function<void(const std::string&)>;
using OnDoneCb = std::function<void(bool, const std::string&)>;
using OnMultiDoneCb = std::function<void(const std::vector<std::string>&)>;

class Conversation : public std::enable_shared_from_this<Conversation>
{
public:
    Conversation(const std::weak_ptr<JamiAccount>& account,
                 ConversationMode mode,
                 const std::string& otherMember = "");
    Conversation(const std::weak_ptr<JamiAccount>& account, const std::string& conversationId = "");
    Conversation(const std::weak_ptr<JamiAccount>& account,
                 const std::string& remoteDevice,
                 const std::string& conversationId);
    ~Conversation();

    /**
     * Refresh active calls.
     * @note: If the host crash during a call, when initializing, we need to update
     * and commit all the crashed calls
     * @return  Commits added
     */
    std::vector<std::string> refreshActiveCalls();

    /**
     * Add a callback to update upper layers
     * @note to call after the construction (and before ConversationReady)
     * @param lastDisplayedUpdatedCb    Triggered when last displayed for account is updated
     */
    void onLastDisplayedUpdated(
        std::function<void(const std::string&, const std::string&)>&& lastDisplayedUpdatedCb);

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
    std::vector<std::string> memberUris(
        std::string_view filter = {},
        const std::set<MemberRole>& filteredRoles = {MemberRole::INVITED,
                                                     MemberRole::LEFT,
                                                     MemberRole::BANNED}) const;

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
    void loadMessages(const OnLoadMessages& cb, const LogOptions& options);
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
     */
    void pull(const std::string& deviceId, OnPullCb&& cb, std::string commitId = "");
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
     * Reset fetched informations
     */
    void clearFetched();
    /**
     * Check if a device has fetched last commit
     * @param deviceId
     */
    bool needsFetch(const std::string& deviceId) const;
    /**
     * Store informations about who fetch or not. This simplify sync (sync when a device without the
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
     * Compute, with multi device support the last message displayed of a conversation
     * @param lastDisplayed      Latest displayed interaction
     */
    void updateLastDisplayed(const std::string& lastDisplayed);

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

    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace jami
