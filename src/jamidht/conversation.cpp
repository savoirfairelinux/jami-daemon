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
#include "conversation.h"

#include "account_const.h"
#include "fileutils.h"
#include "jamiaccount.h"
#include "client/ring_signal.h"

#include <charconv>
#include <json/json.h>
#include <string_view>
#include <opendht/thread_pool.h>
#include <tuple>
#include <optional>
#include "swarm/swarm_manager.h"
#ifdef ENABLE_PLUGIN
#include "manager.h"
#include "plugin/jamipluginmanager.h"
#include "plugin/streamdata.h"
#endif
#include "jami/conversation_interface.h"

namespace jami {

static const char* const LAST_MODIFIED = "lastModified";
static const auto jsonBuilder = [] {
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return wbuilder;
}();

ConvInfo::ConvInfo(const Json::Value& json)
{
    id = json[ConversationMapKeys::ID].asString();
    created = json[ConversationMapKeys::CREATED].asLargestUInt();
    removed = json[ConversationMapKeys::REMOVED].asLargestUInt();
    erased = json[ConversationMapKeys::ERASED].asLargestUInt();
    for (const auto& v : json[ConversationMapKeys::MEMBERS]) {
        members.emplace(v["uri"].asString());
    }
    lastDisplayed = json[ConversationMapKeys::LAST_DISPLAYED].asString();
}

Json::Value
ConvInfo::toJson() const
{
    Json::Value json;
    json[ConversationMapKeys::ID] = id;
    json[ConversationMapKeys::CREATED] = Json::Int64(created);
    if (removed) {
        json[ConversationMapKeys::REMOVED] = Json::Int64(removed);
    }
    if (erased) {
        json[ConversationMapKeys::ERASED] = Json::Int64(erased);
    }
    for (const auto& m : members) {
        Json::Value member;
        member["uri"] = m;
        json[ConversationMapKeys::MEMBERS].append(member);
    }
    json[ConversationMapKeys::LAST_DISPLAYED] = lastDisplayed;
    return json;
}

// ConversationRequest
ConversationRequest::ConversationRequest(const Json::Value& json)
{
    received = json[ConversationMapKeys::RECEIVED].asLargestUInt();
    declined = json[ConversationMapKeys::DECLINED].asLargestUInt();
    from = json[ConversationMapKeys::FROM].asString();
    conversationId = json[ConversationMapKeys::CONVERSATIONID].asString();
    auto& md = json[ConversationMapKeys::METADATAS];
    for (const auto& member : md.getMemberNames()) {
        metadatas.emplace(member, md[member].asString());
    }
}

Json::Value
ConversationRequest::toJson() const
{
    Json::Value json;
    json[ConversationMapKeys::CONVERSATIONID] = conversationId;
    json[ConversationMapKeys::FROM] = from;
    json[ConversationMapKeys::RECEIVED] = static_cast<uint32_t>(received);
    if (declined)
        json[ConversationMapKeys::DECLINED] = static_cast<uint32_t>(declined);
    for (const auto& [key, value] : metadatas) {
        json[ConversationMapKeys::METADATAS][key] = value;
    }
    return json;
}

std::map<std::string, std::string>
ConversationRequest::toMap() const
{
    auto result = metadatas;
    result[ConversationMapKeys::ID] = conversationId;
    result[ConversationMapKeys::FROM] = from;
    if (declined)
        result[ConversationMapKeys::DECLINED] = std::to_string(declined);
    result[ConversationMapKeys::RECEIVED] = std::to_string(received);
    return result;
}

using MessageList = std::list<std::shared_ptr<libjami::SwarmMessage>>;

struct History
{
    // While loading the history, we need to avoid:
    // - reloading history (can just be ignored)
    // - adding new commits (should wait for history to be loaded)
    std::mutex mutex {};
    std::condition_variable cv {};
    bool loading {false};
    MessageList messageList {};
    std::map<std::string, std::shared_ptr<libjami::SwarmMessage>> quickAccess {};
    std::map<std::string, std::list<std::shared_ptr<libjami::SwarmMessage>>> pendingEditions {};
    std::map<std::string, std::list<std::map<std::string, std::string>>> pendingReactions {};
};

class Conversation::Impl
{
public:
    Impl(const std::shared_ptr<JamiAccount>& account,
         ConversationMode mode,
         const std::string& otherMember = "")
        : repository_(ConversationRepository::createConversation(account, mode, otherMember))
        , account_(account)
        , accountId_(account->getAccountID())
        , userId_(account->getUsername())
        , deviceId_(account->currentDeviceId())
    {
        if (!repository_) {
            throw std::logic_error("Unable to create repository");
        }
        init(account);
    }

    Impl(const std::shared_ptr<JamiAccount>& account, const std::string& conversationId)
        : account_(account)
        , accountId_(account->getAccountID())
        , userId_(account->getUsername())
        , deviceId_(account->currentDeviceId())
    {
        repository_ = std::make_unique<ConversationRepository>(account, conversationId);
        if (!repository_) {
            throw std::logic_error("Unable to create repository");
        }
        init(account);
    }

    Impl(const std::shared_ptr<JamiAccount>& account,
         const std::string& remoteDevice,
         const std::string& conversationId)
        : account_(account)
        , accountId_(account->getAccountID())
        , userId_(account->getUsername())
        , deviceId_(account->currentDeviceId())
    {
        std::vector<ConversationCommit> commits;
        repository_ = ConversationRepository::cloneConversation(account,
                                                                remoteDevice,
                                                                conversationId,
                                                                [&](auto c) {
                                                                    commits = std::move(c);
                                                                });
        if (!repository_) {
            emitSignal<libjami::ConversationSignal::OnConversationError>(
                accountId_, conversationId, EFETCH, "Unable to clone repository");
            throw std::logic_error("Unable to clone repository");
        }
        // To detect current active calls, we need to check history
        conversationDataPath_ = fileutils::get_data_dir() / accountId_
                                / "conversation_data" / conversationId;
        activeCallsPath_ = conversationDataPath_ / ConversationMapKeys::ACTIVE_CALLS;
        initActiveCalls(repository_->convCommitsToMap(commits));
        init(account);
    }

    void init(const std::shared_ptr<JamiAccount>& account) {
        ioContext_ = Manager::instance().ioContext();
        fallbackTimer_ = std::make_unique<asio::steady_timer>(*ioContext_);
        swarmManager_
            = std::make_shared<SwarmManager>(NodeId(deviceId_),
                                             Manager::instance().getSeededRandomEngine(),
                                            [account = account_](const DeviceId& deviceId) {
                                                if (auto acc = account.lock()) {
                                                    return acc->isConnectedWith(deviceId);
                                                }
                                                return false;
                                            });
        swarmManager_->setMobility(account->isMobile());
        transferManager_
            = std::make_shared<TransferManager>(accountId_,
                                                "",
                                                repository_->id(),
                                                Manager::instance().getSeededRandomEngine());
        conversationDataPath_ = fileutils::get_data_dir() / accountId_
                                / "conversation_data" / repository_->id();
        fetchedPath_ = conversationDataPath_ / "fetched";
        statusPath_ = conversationDataPath_ / "status";
        sendingPath_ = conversationDataPath_ / "sending";
        preferencesPath_ = conversationDataPath_ / ConversationMapKeys::PREFERENCES;
        activeCallsPath_ = conversationDataPath_ / ConversationMapKeys::ACTIVE_CALLS;
        hostedCallsPath_ = conversationDataPath_ / ConversationMapKeys::HOSTED_CALLS;
        loadActiveCalls();
        loadStatus();
        typers_ = std::make_shared<Typers>(account, repository_->id());
    }

    const std::string& toString() const
    {
        if (fmtStr_.empty()) {
            if (repository_->mode() == ConversationMode::ONE_TO_ONE) {
                auto peer = userId_;
                for (const auto& member : repository_->getInitialMembers()) {
                    if (member != userId_) {
                        peer = member;
                    }
                }
                fmtStr_ = fmt::format("[Account {}] [Conversation (1:1) {}]", accountId_, peer);
            } else {
                fmtStr_ = fmt::format("[Account {}] [Conversation {}]", accountId_, repository_->id());
            }
        }
        return fmtStr_;
    }
    mutable std::string fmtStr_;

    ~Impl()
    {
        try {
            if (fallbackTimer_)
                fallbackTimer_->cancel();
        } catch (const std::exception& e) {
            JAMI_ERROR("{:s} {:s}", toString(), e.what());
        }
    }

    /**
     * If, for whatever reason, the daemon is stopped while hosting a conference,
     * we need to announce the end of this call when restarting.
     * To avoid to keep active calls forever.
     */
    std::vector<std::string> commitsEndedCalls();
    bool isAdmin() const;
    std::filesystem::path repoPath() const;

    void announce(const std::string& commitId, bool commitFromSelf = false)
    {
        std::vector<std::string> vec;
        if (!commitId.empty())
            vec.emplace_back(commitId);
        announce(vec, commitFromSelf);
    }

    void announce(const std::vector<std::string>& commits, bool commitFromSelf = false)
    {
        std::vector<ConversationCommit> convcommits;
        convcommits.reserve(commits.size());
        for (const auto& cid : commits) {
            auto commit = repository_->getCommit(cid);
            if (commit != std::nullopt) {
                convcommits.emplace_back(*commit);
            }
        }
        announce(repository_->convCommitsToMap(convcommits), commitFromSelf);
    }

    /**
     * Initialize activeCalls_ from the list of commits in the repository
     * @param commits Commits in reverse chronological order (i.e. from newest to oldest)
     */
    void initActiveCalls(const std::vector<std::map<std::string, std::string>>& commits) const
    {
        std::unordered_set<std::string> invalidHostUris;
        std::unordered_set<std::string> invalidCallIds;

        std::lock_guard lk(activeCallsMtx_);
        for (const auto& commit : commits) {
            if (commit.at("type") == "member") {
                // Each commit of type "member" has an "action" field whose value can be one
                // of the following: "add", "join", "remove", "ban", "unban"
                // In the case of "remove" and "ban", we need to add the member's URI to
                // invalidHostUris to ensure that any call they may have started in the past
                // is no longer considered active.
                // For the other actions, there's no harm in adding the member's URI anyway,
                // since it's not possible to start hosting a call before joining the swarm (or
                // before getting unbanned in the case of previously banned members).
                invalidHostUris.emplace(commit.at("uri"));
            } else if (commit.find("confId") != commit.end() && commit.find("uri") != commit.end()
                       && commit.find("device") != commit.end()) {
                // The commit indicates either the end or the beginning of a call
                // (depending on whether there is a "duration" field or not).
                auto convId = repository_->id();
                auto confId = commit.at("confId");
                auto uri = commit.at("uri");
                auto device = commit.at("device");

                if (commit.find("duration") == commit.end()
                    && invalidCallIds.find(confId) == invalidCallIds.end()
                    && invalidHostUris.find(uri) == invalidHostUris.end()) {
                    std::map<std::string, std::string> activeCall;
                    activeCall["id"] = confId;
                    activeCall["uri"] = uri;
                    activeCall["device"] = device;
                    activeCalls_.emplace_back(activeCall);
                    fmt::print("swarm:{} new active call detected: {} (on device {}, account {})\n",
                               convId,
                               confId,
                               device,
                               uri);
                }
                // Even if the call was active, we still add its ID to invalidCallIds to make sure it
                // doesn't get added a second time. (This shouldn't happen normally, but in practice
                // there are sometimes multiple commits indicating the beginning of the same call.)
                invalidCallIds.emplace(confId);
            }
        }
        saveActiveCalls();
        emitSignal<libjami::ConfigurationSignal::ActiveCallsChanged>(accountId_,
                                                                     repository_->id(),
                                                                     activeCalls_);
    }

    /**
     * Update activeCalls_ via announced commits (in load or via new commits)
     * @param commit        Commit to check
     * @param eraseOnly     If we want to ignore added commits
     * @param emitSig    If we want to emit to client
     * @note eraseOnly is used by loadMessages. This is a fail-safe, this SHOULD NOT happen
     */
    void updateActiveCalls(const std::map<std::string, std::string>& commit,
                           bool eraseOnly = false,
                           bool emitSig = true) const
    {
        if (!repository_)
            return;
        if (commit.at("type") == "member") {
            // In this case, we need to check if we are not removing a hosting member or device
            std::lock_guard lk(activeCallsMtx_);
            auto it = activeCalls_.begin();
            auto updateActives = false;
            while (it != activeCalls_.end()) {
                if (it->at("uri") == commit.at("uri") || it->at("device") == commit.at("uri")) {
                    JAMI_DEBUG("Removing {:s} from the active calls, because {:s} left",
                               it->at("id"),
                               commit.at("uri"));
                    it = activeCalls_.erase(it);
                    updateActives = true;
                } else {
                    ++it;
                }
            }
            if (updateActives) {
                saveActiveCalls();
                if (emitSig)
                    emitSignal<libjami::ConfigurationSignal::ActiveCallsChanged>(accountId_,
                                                                                 repository_->id(),
                                                                                 activeCalls_);
            }
            return;
        }
        // Else, it's a call information
        if (commit.find("confId") != commit.end() && commit.find("uri") != commit.end()
            && commit.find("device") != commit.end()) {
            auto convId = repository_->id();
            auto confId = commit.at("confId");
            auto uri = commit.at("uri");
            auto device = commit.at("device");
            std::lock_guard lk(activeCallsMtx_);
            auto itActive = std::find_if(activeCalls_.begin(),
                                         activeCalls_.end(),
                                         [&](const auto& value) {
                                             return value.at("id") == confId
                                                    && value.at("uri") == uri
                                                    && value.at("device") == device;
                                         });
            if (commit.find("duration") == commit.end()) {
                if (itActive == activeCalls_.end() && !eraseOnly) {
                    JAMI_DEBUG(
                        "swarm:{:s} new current call detected: {:s} on device {:s}, account {:s}",
                        convId,
                        confId,
                        device,
                        uri);
                    std::map<std::string, std::string> activeCall;
                    activeCall["id"] = confId;
                    activeCall["uri"] = uri;
                    activeCall["device"] = device;
                    activeCalls_.emplace_back(activeCall);
                    saveActiveCalls();
                    if (emitSig)
                        emitSignal<libjami::ConfigurationSignal::ActiveCallsChanged>(accountId_,
                                                                                     repository_
                                                                                         ->id(),
                                                                                     activeCalls_);
                }
            } else {
                if (itActive != activeCalls_.end()) {
                    itActive = activeCalls_.erase(itActive);
                    // Unlikely, but we must ensure that no duplicate exists
                    while (itActive != activeCalls_.end()) {
                        itActive = std::find_if(itActive, activeCalls_.end(), [&](const auto& value) {
                            return value.at("id") == confId && value.at("uri") == uri
                                   && value.at("device") == device;
                        });
                        if (itActive != activeCalls_.end()) {
                            JAMI_ERROR("Duplicate call found. (This is a bug)");
                            itActive = activeCalls_.erase(itActive);
                        }
                    }

                    if (eraseOnly) {
                        JAMI_WARNING("previous swarm:{:s} call finished detected: {:s} on device "
                                     "{:s}, account {:s}",
                                     convId,
                                     confId,
                                     device,
                                     uri);
                    } else {
                        JAMI_DEBUG("swarm:{:s} call finished: {:s} on device {:s}, account {:s}",
                                   convId,
                                   confId,
                                   device,
                                   uri);
                    }
                }
                saveActiveCalls();
                if (emitSig)
                    emitSignal<libjami::ConfigurationSignal::ActiveCallsChanged>(accountId_,
                                                                                 repository_->id(),
                                                                                 activeCalls_);
            }
        }
    }

    void announce(const std::vector<std::map<std::string, std::string>>& commits, bool commitFromSelf = false)
    {
        if (!repository_)
            return;
        auto convId = repository_->id();
        auto ok = !commits.empty();
        auto lastId = ok ? commits.rbegin()->at(ConversationMapKeys::ID) : "";
        addToHistory(loadedHistory_, commits, true, commitFromSelf);
        if (ok) {
            bool announceMember = false;
            for (const auto& c : commits) {
                // Announce member events
                if (c.at("type") == "member") {
                    if (c.find("uri") != c.end() && c.find("action") != c.end()) {
                        const auto& uri = c.at("uri");
                        const auto& actionStr = c.at("action");
                        auto action = -1;
                        if (actionStr == "add")
                            action = 0;
                        else if (actionStr == "join")
                            action = 1;
                        else if (actionStr == "remove")
                            action = 2;
                        else if (actionStr == "ban")
                            action = 3;
                        else if (actionStr == "unban")
                            action = 4;
                        if (actionStr == "ban" || actionStr == "remove") {
                            // In this case, a potential host was removed during a call.
                            updateActiveCalls(c);
                            typers_->removeTyper(uri);
                        }
                        if (action != -1) {
                            announceMember = true;
                            emitSignal<libjami::ConversationSignal::ConversationMemberEvent>(
                                accountId_, convId, uri, action);
                        }
                    }
                } else if (c.at("type") == "application/call-history+json") {
                    updateActiveCalls(c);
                }
#ifdef ENABLE_PLUGIN
                auto& pluginChatManager
                    = Manager::instance().getJamiPluginManager().getChatServicesManager();
                if (pluginChatManager.hasHandlers()) {
                    auto cm = std::make_shared<JamiMessage>(accountId_,
                                                            convId,
                                                            c.at("author") != userId_,
                                                            c,
                                                            false);
                    cm->isSwarm = true;
                    pluginChatManager.publishMessage(std::move(cm));
                }
#endif
                // announce message
                emitSignal<libjami::ConversationSignal::MessageReceived>(accountId_, convId, c);
            }

            if (announceMember && onMembersChanged_) {
                onMembersChanged_(repository_->memberUris("", {}));
            }
        }
    }

    void loadStatus()
    {
        try {
            // read file
            auto file = fileutils::loadFile(statusPath_);
            // load values
            msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
            std::lock_guard lk {messageStatusMtx_};
            oh.get().convert(messagesStatus_);
        } catch (const std::exception& e) {
        }
    }
    void saveStatus()
    {
        std::ofstream file(statusPath_, std::ios::trunc | std::ios::binary);
        msgpack::pack(file, messagesStatus_);
    }

    void loadActiveCalls() const
    {
        try {
            // read file
            auto file = fileutils::loadFile(activeCallsPath_);
            // load values
            msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
            std::lock_guard lk {activeCallsMtx_};
            oh.get().convert(activeCalls_);
        } catch (const std::exception& e) {
            return;
        }
    }

    void saveActiveCalls() const
    {
        std::ofstream file(activeCallsPath_, std::ios::trunc | std::ios::binary);
        msgpack::pack(file, activeCalls_);
    }

    void loadHostedCalls() const
    {
        try {
            // read file
            auto file = fileutils::loadFile(hostedCallsPath_);
            // load values
            msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
            std::lock_guard lk {activeCallsMtx_};
            oh.get().convert(hostedCalls_);
        } catch (const std::exception& e) {
            return;
        }
    }

    void saveHostedCalls() const
    {
        std::ofstream file(hostedCallsPath_, std::ios::trunc | std::ios::binary);
        msgpack::pack(file, hostedCalls_);
    }

    void voteUnban(const std::string& contactUri, const std::string_view type, const OnDoneCb& cb);

    std::vector<std::map<std::string, std::string>> getMembers(bool includeInvited,
                                                               bool includeLeft,
                                                               bool includeBanned) const;

    std::string_view bannedType(const std::string& uri) const
    {
        auto repo = repoPath();
        auto crt = fmt::format("{}.crt", uri);
        auto bannedMember = repo / "banned" / "members" / crt;
        if (std::filesystem::is_regular_file(bannedMember))
            return "members"sv;
        auto bannedAdmin = repo / "banned" / "admins" / crt;
        if (std::filesystem::is_regular_file(bannedAdmin))
            return "admins"sv;
        auto bannedInvited = repo / "banned" / "invited" / uri;
        if (std::filesystem::is_regular_file(bannedInvited))
            return "invited"sv;
        auto bannedDevice = repo / "banned" / "devices" / crt;
        if (std::filesystem::is_regular_file(bannedDevice))
            return "devices"sv;
        return {};
    }

    std::shared_ptr<dhtnet::ChannelSocket> gitSocket(const DeviceId& deviceId) const
    {
        auto deviceSockets = gitSocketList_.find(deviceId);
        return (deviceSockets != gitSocketList_.end()) ? deviceSockets->second : nullptr;
    }

    void addGitSocket(const DeviceId& deviceId, const std::shared_ptr<dhtnet::ChannelSocket>& socket)
    {
        gitSocketList_[deviceId] = socket;
    }
    void removeGitSocket(const DeviceId& deviceId)
    {
        auto deviceSockets = gitSocketList_.find(deviceId);
        if (deviceSockets != gitSocketList_.end())
            gitSocketList_.erase(deviceSockets);
    }

    /**
     * Remove all git sockets and all DRT nodes associated with the given peer.
     * This is used when a swarm member is banned to ensure that we stop syncing
     * with them or sending them message notifications.
     */
    void disconnectFromPeer(const std::string& peerUri);

    std::vector<std::map<std::string, std::string>> getMembers(bool includeInvited,
                                                               bool includeLeft) const;

    std::mutex membersMtx_ {};
    std::set<std::string> checkedMembers_; // Store members we tried
    std::function<void()> bootstrapCb_;
#ifdef LIBJAMI_TESTABLE
    std::function<void(std::string, BootstrapStatus)> bootstrapCbTest_;
#endif

    std::mutex writeMtx_ {};
    std::unique_ptr<ConversationRepository> repository_;
    std::shared_ptr<SwarmManager> swarmManager_;
    std::weak_ptr<JamiAccount> account_;
    std::string accountId_ {};
    std::string userId_;
    std::string deviceId_;
    std::atomic_bool isRemoving_ {false};
    std::vector<std::map<std::string, std::string>> loadMessages(const LogOptions& options);
    std::vector<libjami::SwarmMessage> loadMessages2(const LogOptions& options,
                                                     History* optHistory = nullptr);
    void pull(const std::string& deviceId);
    std::vector<std::map<std::string, std::string>> mergeHistory(const std::string& uri);

    // Avoid multiple fetch/merges at the same time.
    std::mutex pullcbsMtx_ {};
    std::map<std::string, std::deque<std::pair<std::string, OnPullCb>>> fetchingRemotes_ {}; // store current remote in fetch
    std::shared_ptr<TransferManager> transferManager_ {};
    std::filesystem::path conversationDataPath_ {};
    std::filesystem::path fetchedPath_ {};

    // Manage last message displayed and status
    std::filesystem::path sendingPath_ {};
    std::filesystem::path preferencesPath_ {};
    OnMembersChanged onMembersChanged_ {};

    // Manage hosted calls on this device
    std::filesystem::path hostedCallsPath_ {};
    mutable std::map<std::string, uint64_t /* start time */> hostedCalls_ {};
    // Manage active calls for this conversation (can be hosted by other devices)
    std::filesystem::path activeCallsPath_ {};
    mutable std::mutex activeCallsMtx_ {};
    mutable std::vector<std::map<std::string, std::string>> activeCalls_ {};

    GitSocketList gitSocketList_ {};

    // Bootstrap
    std::shared_ptr<asio::io_context> ioContext_;
    std::unique_ptr<asio::steady_timer> fallbackTimer_;


    /**
     * Loaded history represents the linearized history to show for clients
     */
    History loadedHistory_ {};
    std::vector<std::shared_ptr<libjami::SwarmMessage>> addToHistory(
        History& history,
        const std::vector<std::map<std::string, std::string>>& commits,
        bool messageReceived = false,
        bool commitFromSelf = false);

    void handleReaction(History& history,
                        const std::shared_ptr<libjami::SwarmMessage>& sharedCommit) const;
    void handleEdition(History& history,
                       const std::shared_ptr<libjami::SwarmMessage>& sharedCommit,
                       bool messageReceived) const;
    bool handleMessage(History& history,
                       const std::shared_ptr<libjami::SwarmMessage>& sharedCommit,
                       bool messageReceived) const;
    void rectifyStatus(const std::shared_ptr<libjami::SwarmMessage>& message,
                       History& history) const;
    /**
     * {uri, {
     *          {"fetch", "commitId"},
     *          {"fetched_ts", "timestamp"},
     *          {"read", "commitId"},
     *          {"read_ts", "timestamp"}
     *       }
     * }
     */
    mutable std::mutex messageStatusMtx_;
    std::function<void(const std::map<std::string, std::map<std::string, std::string>>&)> messageStatusCb_ {};
    std::filesystem::path statusPath_ {};
    mutable std::map<std::string, std::map<std::string, std::string>> messagesStatus_ {};
    /**
     * Status: 0 = commited, 1 = fetched, 2 = read
     * This cache the curent status to add in the messages
     */
    // Note: only store int32_t cause it's easy to pass to dbus this way
    // memberToStatus serves as a cache for loading messages
    mutable std::map<std::string, int32_t> memberToStatus;


    // futureStatus is used to store the status for receiving messages
    // (because we're not sure to fetch the commit before receiving a status change for this)
    mutable std::map<std::string, std::map<std::string, int32_t>> futureStatus;
    // Update internal structures regarding status
    void updateStatus(const std::string& uri,
                      libjami::Account::MessageStates status,
                      const std::string& commitId,
                      const std::string& ts,
                      bool emit = false);


    std::shared_ptr<Typers> typers_;
};

bool
Conversation::Impl::isAdmin() const
{
    auto adminsPath = repoPath() / "admins";
    return std::filesystem::is_regular_file(fileutils::getFullPath(adminsPath, userId_ + ".crt"));
}

void
Conversation::Impl::disconnectFromPeer(const std::string& peerUri)
{
    // Remove nodes from swarmManager
    const auto nodes = swarmManager_->getRoutingTable().getAllNodes();
    std::vector<NodeId> toRemove;
    for (const auto node : nodes)
        if (peerUri == repository_->uriFromDevice(node.toString()))
            toRemove.emplace_back(node);
    swarmManager_->deleteNode(toRemove);

    // Remove git sockets with this member
    for (auto it = gitSocketList_.begin(); it != gitSocketList_.end();) {
        if (peerUri == repository_->uriFromDevice(it->first.toString()))
            it = gitSocketList_.erase(it);
        else
            ++it;
    }
}

std::vector<std::map<std::string, std::string>>
Conversation::Impl::getMembers(bool includeInvited, bool includeLeft, bool includeBanned) const
{
    std::vector<std::map<std::string, std::string>> result;
    auto members = repository_->members();
    std::lock_guard lk(messageStatusMtx_);
    for (const auto& member : members) {
        if (member.role == MemberRole::BANNED && !includeBanned) {
            continue;
        }
        if (member.role == MemberRole::INVITED && !includeInvited)
            continue;
        if (member.role == MemberRole::LEFT && !includeLeft)
            continue;
        auto mm = member.map();
        mm[ConversationMapKeys::LAST_DISPLAYED] = messagesStatus_[member.uri]["read"];
        result.emplace_back(std::move(mm));
    }
    return result;
}

std::vector<std::string>
Conversation::Impl::commitsEndedCalls()
{
    // Handle current calls
    std::vector<std::string> commits {};
    std::unique_lock lk(writeMtx_);
    std::unique_lock lkA(activeCallsMtx_);
    for (const auto& hostedCall : hostedCalls_) {
        // In this case, this means that we left
        // the conference while still hosting it, so activeCalls
        // will not be correctly updated
        // We don't need to send notifications there, as peers will sync with presence
        Json::Value value;
        value["uri"] = userId_;
        value["device"] = deviceId_;
        value["confId"] = hostedCall.first;
        value["type"] = "application/call-history+json";
        auto now = std::chrono::system_clock::now();
        auto nowConverted = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
                                .count();
        value["duration"] = std::to_string((nowConverted - hostedCall.second) * 1000);
        auto itActive = std::find_if(activeCalls_.begin(),
                                     activeCalls_.end(),
                                     [this, confId = hostedCall.first](const auto& value) {
                                         return value.at("id") == confId && value.at("uri") == userId_
                                                && value.at("device") == deviceId_;
                                     });
        if (itActive != activeCalls_.end())
            activeCalls_.erase(itActive);
        commits.emplace_back(repository_->commitMessage(Json::writeString(jsonBuilder, value)));

        JAMI_DEBUG("Removing hosted conference... {:s}", hostedCall.first);
    }
    hostedCalls_.clear();
    saveActiveCalls();
    saveHostedCalls();
    return commits;
}

std::filesystem::path
Conversation::Impl::repoPath() const
{
    return fileutils::get_data_dir() / accountId_ / "conversations" / repository_->id();
}

std::vector<std::map<std::string, std::string>>
Conversation::Impl::loadMessages(const LogOptions& options)
{
    if (!repository_)
        return {};
    std::vector<ConversationCommit> commits;
    auto startLogging = options.from == "";
    auto breakLogging = false;
    repository_->log(
        [&](const auto& id, const auto& author, const auto& commit) {
            if (!commits.empty()) {
                // Set linearized parent
                commits.rbegin()->linearized_parent = id;
            }
            if (options.skipMerge && git_commit_parentcount(commit.get()) > 1) {
                return CallbackResult::Skip;
            }
            if ((options.nbOfCommits != 0 && commits.size() == options.nbOfCommits))
                return CallbackResult::Break; // Stop logging
            if (breakLogging)
                return CallbackResult::Break; // Stop logging
            if (id == options.to) {
                if (options.includeTo)
                    breakLogging = true; // For the next commit
                else
                    return CallbackResult::Break; // Stop logging
            }

            if (!startLogging && options.from != "" && options.from == id)
                startLogging = true;
            if (!startLogging)
                return CallbackResult::Skip; // Start logging after this one

            if (options.fastLog) {
                if (options.authorUri != "") {
                    if (options.authorUri == repository_->uriFromDevice(author.email)) {
                        return CallbackResult::Break; // Found author, stop
                    }
                }
                // Used to only count commit
                commits.emplace(commits.end(), ConversationCommit {});
                return CallbackResult::Skip;
            }

            return CallbackResult::Ok; // Continue
        },
        [&](auto&& cc) { commits.emplace(commits.end(), std::forward<decltype(cc)>(cc)); },
        [](auto, auto, auto) { return false; },
        options.from,
        options.logIfNotFound);
    return repository_->convCommitsToMap(commits);
}

std::vector<libjami::SwarmMessage>
Conversation::Impl::loadMessages2(const LogOptions& options, History* optHistory)
{
    auto history = optHistory ? optHistory : &loadedHistory_;

    // history->mutex is locked by the caller
    if (!repository_ || history->loading) {
        return {};
    }
    history->loading = true;

    // By convention, if options.nbOfCommits is zero, then we
    // don't impose a limit on the number of commits returned.
    bool limitNbOfCommits = options.nbOfCommits > 0;

    auto startLogging = options.from == "";
    auto breakLogging = false;
    auto currentHistorySize = loadedHistory_.messageList.size();
    std::vector<std::string> replies;
    std::vector<std::shared_ptr<libjami::SwarmMessage>> msgList;
    repository_->log(
        /* preCondition */
        [&](const auto& id, const auto& author, const auto& commit) {
            if (options.skipMerge && git_commit_parentcount(commit.get()) > 1) {
                return CallbackResult::Skip;
            }
            if (id == options.to) {
                if (options.includeTo)
                    breakLogging = true; // For the next commit
            }
            if (replies.empty()) { // This avoid load until
                // NOTE: in the future, we may want to add "Reply-Body" in commit to avoid to load
                // until this commit
                if ((limitNbOfCommits
                     && (loadedHistory_.messageList.size() - currentHistorySize)
                            == options.nbOfCommits))
                    return CallbackResult::Break; // Stop logging
                if (breakLogging)
                    return CallbackResult::Break; // Stop logging
                if (id == options.to && !options.includeTo) {
                        return CallbackResult::Break; // Stop logging
                }
            }

            if (!startLogging && options.from != "" && options.from == id)
                startLogging = true;
            if (!startLogging)
                return CallbackResult::Skip; // Start logging after this one

            if (options.fastLog) {
                if (options.authorUri != "") {
                    if (options.authorUri == repository_->uriFromDevice(author.email)) {
                        return CallbackResult::Break; // Found author, stop
                    }
                }
            }

            return CallbackResult::Ok; // Continue
        },
        /* emplaceCb */
        [&](auto&& cc) {
            if(limitNbOfCommits && (msgList.size() == options.nbOfCommits))
                return;
            auto optMessage = repository_->convCommitToMap(cc);
            if (!optMessage.has_value())
                return;
            auto message = optMessage.value();
            if (message.find("reply-to") != message.end()) {
                auto it = std::find(replies.begin(), replies.end(), message.at("reply-to"));
                if(it == replies.end()) {
                    replies.emplace_back(message.at("reply-to"));
                }
            }
            auto it = std::find(replies.begin(), replies.end(), message.at("id"));
            if (it != replies.end()) {
                replies.erase(it);
            }
            std::shared_ptr<libjami::SwarmMessage> firstMsg;
            if ((history == &loadedHistory_) && msgList.empty() && !loadedHistory_.messageList.empty()) {
                firstMsg = *loadedHistory_.messageList.rbegin();
            }
            auto added = addToHistory(*history, {message}, false, false);
            if (!added.empty() && firstMsg) {
                emitSignal<libjami::ConversationSignal::SwarmMessageUpdated>(accountId_,
                                                                             repository_->id(),
                                                                             *firstMsg);
            }
            msgList.insert(msgList.end(), added.begin(), added.end());
        },
        /* postCondition */
        [&](auto, auto, auto) {
            // Stop logging if there was a limit set on the number of commits
            // to return and we reached it. This isn't strictly necessary since
            // the check at the beginning of `emplaceCb` ensures that we won't
            // return too many messages, but it prevents us from needlessly
            // iterating over a (potentially) large number of commits.
            return limitNbOfCommits && (msgList.size() == options.nbOfCommits);
        },
        options.from,
        options.logIfNotFound);

    history->loading = false;
    history->cv.notify_all();

    // Convert for client (remove ptr)
    std::vector<libjami::SwarmMessage> ret;
    ret.reserve(msgList.size());
    for (const auto& msg: msgList) {
        ret.emplace_back(*msg);
    }
    return ret;
}

void
Conversation::Impl::handleReaction(History& history,
                                   const std::shared_ptr<libjami::SwarmMessage>& sharedCommit) const
{
    auto it = history.quickAccess.find(sharedCommit->body.at("react-to"));
    auto peditIt = history.pendingEditions.find(sharedCommit->id);
    if (peditIt != history.pendingEditions.end()) {
        auto oldBody = sharedCommit->body;
        sharedCommit->body["body"] = peditIt->second.front()->body["body"];
        if (sharedCommit->body.at("body").empty())
            return;
        history.pendingEditions.erase(peditIt);
    }
    if (it != history.quickAccess.end()) {
        it->second->reactions.emplace_back(sharedCommit->body);
        emitSignal<libjami::ConversationSignal::ReactionAdded>(accountId_,
                                                               repository_->id(),
                                                               it->second->id,
                                                               sharedCommit->body);
    } else {
        history.pendingReactions[sharedCommit->body.at("react-to")].emplace_back(sharedCommit->body);
    }
}

void
Conversation::Impl::handleEdition(History& history,
                                  const std::shared_ptr<libjami::SwarmMessage>& sharedCommit,
                                  bool messageReceived) const
{
    auto editId = sharedCommit->body.at("edit");
    auto it = history.quickAccess.find(editId);
    if (it != history.quickAccess.end()) {
        auto baseCommit = it->second;
        if (baseCommit) {
            auto itReact = baseCommit->body.find("react-to");
            std::string toReplace = (baseCommit->type == "application/data-transfer+json") ?
                "tid" : "body";
            auto body = sharedCommit->body.at(toReplace);
            // Edit reaction
            if (itReact != baseCommit->body.end()) {
                baseCommit->body[toReplace] = body; // Replace body if pending
                it = history.quickAccess.find(itReact->second);
                auto itPending = history.pendingReactions.find(itReact->second);
                if (it != history.quickAccess.end()) {
                    baseCommit = it->second; // Base commit
                    auto itPreviousReact = std::find_if(baseCommit->reactions.begin(),
                                                        baseCommit->reactions.end(),
                                                        [&](const auto& reaction) {
                                                            return reaction.at("id") == editId;
                                                        });
                    if (itPreviousReact != baseCommit->reactions.end()) {
                        (*itPreviousReact)[toReplace] = body;
                        if (body.empty()) {
                            baseCommit->reactions.erase(itPreviousReact);
                            emitSignal<libjami::ConversationSignal::ReactionRemoved>(accountId_,
                                                                                     repository_
                                                                                         ->id(),
                                                                                     baseCommit->id,
                                                                                     editId);
                        }
                    }
                } else if (itPending != history.pendingReactions.end()) {
                    // Else edit if pending
                    auto itReaction = std::find_if(itPending->second.begin(),
                                                   itPending->second.end(),
                                                   [&](const auto& reaction) {
                                                       return reaction.at("id") == editId;
                                                   });
                    if (itReaction != itPending->second.end()) {
                        (*itReaction)[toReplace] = body;
                        if (body.empty())
                            itPending->second.erase(itReaction);
                    }
                } else {
                    // Add to pending edtions
                    messageReceived ? history.pendingEditions[editId].emplace_front(sharedCommit)
                                    : history.pendingEditions[editId].emplace_back(sharedCommit);
                }
            } else {
                // Normal message
                it->second->editions.emplace(it->second->editions.begin(), it->second->body);
                it->second->body[toReplace] = sharedCommit->body[toReplace];
                if (toReplace == "tid") {
                    // Avoid to replace fileId in client
                    it->second->body["fileId"] = "";
                }
                // Remove reactions
                if (sharedCommit->body.at(toReplace).empty())
                    it->second->reactions.clear();
                emitSignal<libjami::ConversationSignal::SwarmMessageUpdated>(accountId_, repository_->id(), *it->second);
            }
        }
    } else {
        messageReceived ? history.pendingEditions[editId].emplace_front(sharedCommit)
                        : history.pendingEditions[editId].emplace_back(sharedCommit);
    }
}

bool
Conversation::Impl::handleMessage(History& history,
                                  const std::shared_ptr<libjami::SwarmMessage>& sharedCommit,
                                  bool messageReceived) const
{
    if (messageReceived) {
        // For a received message, we place it at the beginning of the list
        if (!history.messageList.empty())
            sharedCommit->linearizedParent = (*history.messageList.begin())->id;
        history.messageList.emplace_front(sharedCommit);
    } else {
        // For a loaded message, we load from newest to oldest
        // So we change the parent of the last message.
        if (!history.messageList.empty())
            (*history.messageList.rbegin())->linearizedParent = sharedCommit->id;
        history.messageList.emplace_back(sharedCommit);
    }
    // Handle pending reactions/editions
    auto reactIt = history.pendingReactions.find(sharedCommit->id);
    if (reactIt != history.pendingReactions.end()) {
        for (const auto& commitBody : reactIt->second)
            sharedCommit->reactions.emplace_back(commitBody);
        history.pendingReactions.erase(reactIt);
    }
    auto peditIt = history.pendingEditions.find(sharedCommit->id);
    if (peditIt != history.pendingEditions.end()) {
        auto oldBody = sharedCommit->body;
        if (sharedCommit->type == "application/data-transfer+json") {
            sharedCommit->body["tid"] = peditIt->second.front()->body["tid"];
            sharedCommit->body["fileId"] = "";
        } else {
            sharedCommit->body["body"] = peditIt->second.front()->body["body"];
        }
        peditIt->second.pop_front();
        for (const auto& commit : peditIt->second) {
            sharedCommit->editions.emplace_back(commit->body);
        }
        sharedCommit->editions.emplace_back(oldBody);
        history.pendingEditions.erase(peditIt);
    }
    // Announce to client
    if (messageReceived)
        emitSignal<libjami::ConversationSignal::SwarmMessageReceived>(accountId_,
                                                                      repository_->id(),
                                                                      *sharedCommit);
    return !messageReceived;
}

void Conversation::Impl::rectifyStatus(const std::shared_ptr<libjami::SwarmMessage>& message,
                                       History& history) const
{

    auto parentIt = history.quickAccess.find(message->linearizedParent);
    auto currentMessage = message;

    while(parentIt != history.quickAccess.end()){
        const auto& parent = parentIt->second;
        for (const auto& [peer, value] : message->status) {
            auto parentStatusIt = parent->status.find(peer);
            if (parentStatusIt == parent->status.end() || parentStatusIt->second < value) {
                parent->status[peer] = value;
                emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
                    accountId_,
                    repository_->id(),
                    peer,
                    parent->id,
                    value);
            }
            else if(parentStatusIt->second >= value){
                break;
            }
        }
        currentMessage = parent;
        parentIt = history.quickAccess.find(parent->linearizedParent);
    }
}


std::vector<std::shared_ptr<libjami::SwarmMessage>>
Conversation::Impl::addToHistory(History& history,
                                 const std::vector<std::map<std::string, std::string>>& commits,
                                 bool messageReceived,
                                 bool commitFromSelf)
{
    auto acc = account_.lock();
    if (!acc)
        return {};
    auto username = acc->getUsername();
    if (messageReceived && (&history == &loadedHistory_ && history.loading)) {
        std::unique_lock lk(history.mutex);
        history.cv.wait(lk, [&] { return !history.loading; });
    }
    std::vector<std::shared_ptr<libjami::SwarmMessage>> messages;
    auto addCommit = [&](const auto& commit) {
        auto commitId = commit.at("id");
        if (history.quickAccess.find(commitId) != history.quickAccess.end())
            return; // Already present
        auto typeIt = commit.find("type");
        auto reactToIt = commit.find("react-to");
        auto editIt = commit.find("edit");
        // Nothing to show for the client, skip
        if (typeIt != commit.end() && typeIt->second == "merge")
            return;

        auto sharedCommit = std::make_shared<libjami::SwarmMessage>();
        sharedCommit->fromMapStringString(commit);
        // Set message status based on cache (only on history for client)
        if (!commitFromSelf && &history == &loadedHistory_) {
            std::lock_guard lk(messageStatusMtx_);
            for (const auto& member: repository_->members()) {
                // If we have a status cached, use it
                auto itFuture = futureStatus.find(sharedCommit->id);
                if (itFuture != futureStatus.end()) {
                    sharedCommit->status = std::move(itFuture->second);
                    futureStatus.erase(itFuture);
                    continue;
                }
                // Else we need to compute the status.
                auto& cache = memberToStatus[member.uri];
                if (cache == 0) {
                    // Message is sending, sent or displayed
                    cache = static_cast<int32_t>(libjami::Account::MessageStates::SENDING);
                }
                if (!messageReceived) {
                    // For loading previous messages, there is 3 cases. Last value cached is displayed, so is every previous commits
                    // Else, if last value is sent, we can compare to the last read commit to update the cache
                    // Finally if it's sending, we check last fetched commit
                    if (cache == static_cast<int32_t>(libjami::Account::MessageStates::SENT)) {
                        if (messagesStatus_[member.uri]["read"] == sharedCommit->id) {
                            cache = static_cast<int32_t>(libjami::Account::MessageStates::DISPLAYED);
                        }
                    } else if (cache <= static_cast<int32_t>(libjami::Account::MessageStates::SENDING)) { // SENDING or UNKNOWN
                        // cache can be upgraded to displayed or sent
                        if (messagesStatus_[member.uri]["read"] == sharedCommit->id) {
                            cache = static_cast<int32_t>(libjami::Account::MessageStates::DISPLAYED);
                        } else if (messagesStatus_[member.uri]["fetched"] == sharedCommit->id) {
                            cache = static_cast<int32_t>(libjami::Account::MessageStates::SENT);
                        }
                    }
                    if(static_cast<int32_t>(cache) > sharedCommit->status[member.uri]){
                        sharedCommit->status[member.uri] = static_cast<int32_t>(cache);
                    }
                } else {
                    // If member is author of the message received, they already saw it
                    if (member.uri == commit.at("author")) {
                        // If member is the author of the commit, they are considered as displayed (same for all previous commits)
                        messagesStatus_[member.uri]["read"] = sharedCommit->id;
                        messagesStatus_[member.uri]["fetched"] = sharedCommit->id;
                        sharedCommit->status[commit.at("author")] = static_cast<int32_t>(libjami::Account::MessageStates::DISPLAYED);
                        cache = static_cast<int32_t>(libjami::Account::MessageStates::DISPLAYED);
                        continue;
                    }
                    // For receiving messages, every commit is considered as SENDING, unless we got a update
                    auto status = static_cast<int32_t>(libjami::Account::MessageStates::SENDING);
                    if (messagesStatus_[member.uri]["read"] == sharedCommit->id) {
                        status = static_cast<int32_t>(libjami::Account::MessageStates::DISPLAYED);
                    } else if (messagesStatus_[member.uri]["fetched"] == sharedCommit->id) {
                        status = static_cast<int32_t>(libjami::Account::MessageStates::SENT);
                    }
                    if(static_cast<int32_t>(status) > sharedCommit->status[member.uri]){
                        sharedCommit->status[member.uri] = static_cast<int32_t>(status);
                    }
                }
            }
        }
        history.quickAccess[commitId] = sharedCommit;

        if (reactToIt != commit.end() && !reactToIt->second.empty()) {
            handleReaction(history, sharedCommit);
        } else if (editIt != commit.end() && !editIt->second.empty()) {
            handleEdition(history, sharedCommit, messageReceived);
        } else if (handleMessage(history, sharedCommit, messageReceived)) {
            messages.emplace_back(sharedCommit);
        }
        rectifyStatus(sharedCommit, history);
    };
    std::for_each(commits.begin(), commits.end(), addCommit);

    return messages;
}

Conversation::Conversation(const std::shared_ptr<JamiAccount>& account,
                           ConversationMode mode,
                           const std::string& otherMember)
    : pimpl_ {new Impl {account, mode, otherMember}}
{}

Conversation::Conversation(const std::shared_ptr<JamiAccount>& account,
                           const std::string& conversationId)
    : pimpl_ {new Impl {account, conversationId}}
{}

Conversation::Conversation(const std::shared_ptr<JamiAccount>& account,
                           const std::string& remoteDevice,
                           const std::string& conversationId)
    : pimpl_ {new Impl {account, remoteDevice, conversationId}}
{}

Conversation::~Conversation() {}

std::string
Conversation::id() const
{
    return pimpl_->repository_ ? pimpl_->repository_->id() : "";
}

void
Conversation::addMember(const std::string& contactUri, const OnDoneCb& cb)
{
    try {
        if (mode() == ConversationMode::ONE_TO_ONE) {
            // Only authorize to add left members
            auto initialMembers = getInitialMembers();
            auto it = std::find(initialMembers.begin(), initialMembers.end(), contactUri);
            if (it == initialMembers.end()) {
                JAMI_WARN("Unable to add new member in one to one conversation");
                cb(false, "");
                return;
            }
        }
    } catch (const std::exception& e) {
        JAMI_WARN("Unable to get mode: %s", e.what());
        cb(false, "");
        return;
    }
    if (isMember(contactUri, true)) {
        JAMI_WARN("Unable to add member %s because it's already a member", contactUri.c_str());
        cb(false, "");
        return;
    }
    if (isBanned(contactUri)) {
        if (pimpl_->isAdmin()) {
            dht::ThreadPool::io().run(
                [w = weak(), contactUri = std::move(contactUri), cb = std::move(cb)] {
                    if (auto sthis = w.lock()) {
                        auto members = sthis->pimpl_->repository_->members();
                        auto type = sthis->pimpl_->bannedType(contactUri);
                        if (type.empty()) {
                            cb(false, {});
                            return;
                        }
                        sthis->pimpl_->voteUnban(contactUri, type, cb);
                    }
                });
        } else {
            JAMI_WARN("Unable to add member %s because this member is blocked", contactUri.c_str());
            cb(false, "");
        }
        return;
    }

    dht::ThreadPool::io().run([w = weak(), contactUri = std::move(contactUri), cb = std::move(cb)] {
        if (auto sthis = w.lock()) {
            // Add member files and commit
            std::unique_lock lk(sthis->pimpl_->writeMtx_);
            auto commit = sthis->pimpl_->repository_->addMember(contactUri);
            sthis->pimpl_->announce(commit, true);
            lk.unlock();
            if (cb)
                cb(!commit.empty(), commit);
        }
    });
}

std::shared_ptr<dhtnet::ChannelSocket>
Conversation::gitSocket(const DeviceId& deviceId) const
{
    return pimpl_->gitSocket(deviceId);
}

void
Conversation::addGitSocket(const DeviceId& deviceId,
                           const std::shared_ptr<dhtnet::ChannelSocket>& socket)
{
    pimpl_->addGitSocket(deviceId, socket);
}

void
Conversation::removeGitSocket(const DeviceId& deviceId)
{
    pimpl_->removeGitSocket(deviceId);
}

void
Conversation::shutdownConnections()
{
    pimpl_->fallbackTimer_->cancel();
    pimpl_->gitSocketList_.clear();
    if (pimpl_->swarmManager_)
        pimpl_->swarmManager_->shutdown();
    std::lock_guard lk(pimpl_->membersMtx_);
    pimpl_->checkedMembers_.clear();
}

void
Conversation::connectivityChanged()
{
    if (pimpl_->swarmManager_)
        pimpl_->swarmManager_->maintainBuckets();
}

std::vector<jami::DeviceId>
Conversation::getDeviceIdList() const
{
    return pimpl_->swarmManager_->getRoutingTable().getAllNodes();
}

std::shared_ptr<Typers>
Conversation::typers() const
{
    return pimpl_->typers_;
}

bool
Conversation::hasSwarmChannel(const std::string& deviceId)
{
    if (!pimpl_->swarmManager_)
        return false;
    return pimpl_->swarmManager_->isConnectedWith(DeviceId(deviceId));
}

void
Conversation::Impl::voteUnban(const std::string& contactUri,
                              const std::string_view type,
                              const OnDoneCb& cb)
{
    // Check if admin
    if (!isAdmin()) {
        JAMI_WARN("You're not an admin of this repo. Unable to unblock %s", contactUri.c_str());
        cb(false, {});
        return;
    }

    // Vote for removal
    std::unique_lock lk(writeMtx_);
    auto voteCommit = repository_->voteUnban(contactUri, type);
    if (voteCommit.empty()) {
        JAMI_WARN("Unbanning %s failed", contactUri.c_str());
        cb(false, "");
        return;
    }

    auto lastId = voteCommit;
    std::vector<std::string> commits;
    commits.emplace_back(voteCommit);

    // If admin, check vote
    auto resolveCommit = repository_->resolveVote(contactUri, type, "unban");
    if (!resolveCommit.empty()) {
        commits.emplace_back(resolveCommit);
        lastId = resolveCommit;
        JAMI_WARN("Vote solved for unbanning %s.", contactUri.c_str());
    }
    announce(commits, true);
    lk.unlock();
    if (cb)
        cb(!lastId.empty(), lastId);
}

void
Conversation::removeMember(const std::string& contactUri, bool isDevice, const OnDoneCb& cb)
{
    dht::ThreadPool::io().run([w = weak(),
                               contactUri = std::move(contactUri),
                               isDevice = std::move(isDevice),
                               cb = std::move(cb)] {
        if (auto sthis = w.lock()) {
            // Check if admin
            if (!sthis->pimpl_->isAdmin()) {
                JAMI_WARN("You're not an admin of this repo. Unable to block %s", contactUri.c_str());
                cb(false, {});
                return;
            }

            // Get current user type
            std::string type;
            if (isDevice) {
                type = "devices";
            } else {
                auto members = sthis->pimpl_->repository_->members();
                for (const auto& member : members) {
                    if (member.uri == contactUri) {
                        if (member.role == MemberRole::INVITED) {
                            type = "invited";
                        } else if (member.role == MemberRole::ADMIN) {
                            type = "admins";
                        } else if (member.role == MemberRole::MEMBER) {
                            type = "members";
                        }
                        break;
                    }
                }
                if (type.empty()) {
                    cb(false, {});
                    return;
                }
            }

            // Vote for removal
            std::unique_lock lk(sthis->pimpl_->writeMtx_);
            auto voteCommit = sthis->pimpl_->repository_->voteKick(contactUri, type);
            if (voteCommit.empty()) {
                JAMI_WARN("Kicking %s failed", contactUri.c_str());
                cb(false, "");
                return;
            }

            auto lastId = voteCommit;
            std::vector<std::string> commits;
            commits.emplace_back(voteCommit);

            // If admin, check vote
            auto resolveCommit = sthis->pimpl_->repository_->resolveVote(contactUri, type, "ban");
            if (!resolveCommit.empty()) {
                commits.emplace_back(resolveCommit);
                lastId = resolveCommit;
                JAMI_WARN("Vote solved for %s. %s banned",
                          contactUri.c_str(),
                          isDevice ? "Device" : "Member");
                sthis->pimpl_->disconnectFromPeer(contactUri);
            }

            sthis->pimpl_->announce(commits, true);
            lk.unlock();
            cb(!lastId.empty(), lastId);
        }
    });
}

std::vector<std::map<std::string, std::string>>
Conversation::getMembers(bool includeInvited, bool includeLeft, bool includeBanned) const
{
    return pimpl_->getMembers(includeInvited, includeLeft, includeBanned);
}

std::set<std::string>
Conversation::memberUris(std::string_view filter, const std::set<MemberRole>& filteredRoles) const
{
    return pimpl_->repository_->memberUris(filter, filteredRoles);
}

std::vector<NodeId>
Conversation::peersToSyncWith() const
{
    const auto& routingTable = pimpl_->swarmManager_->getRoutingTable();
    const auto& nodes = routingTable.getNodes();
    const auto& mobiles = routingTable.getMobileNodes();
    std::vector<NodeId> s;
    s.reserve(nodes.size() + mobiles.size());
    s.insert(s.end(), nodes.begin(), nodes.end());
    s.insert(s.end(), mobiles.begin(), mobiles.end());
    for (const auto& [deviceId, _] : pimpl_->gitSocketList_)
        if (std::find(s.cbegin(), s.cend(), deviceId) == s.cend())
            s.emplace_back(deviceId);
    return s;
}

bool
Conversation::isBootstraped() const
{
    const auto& routingTable = pimpl_->swarmManager_->getRoutingTable();
    return !routingTable.getNodes().empty();
}

std::string
Conversation::uriFromDevice(const std::string& deviceId) const
{
    return pimpl_->repository_->uriFromDevice(deviceId);
}

void
Conversation::monitor()
{
    pimpl_->swarmManager_->getRoutingTable().printRoutingTable();
}

std::string
Conversation::join()
{
    return pimpl_->repository_->join();
}

bool
Conversation::isMember(const std::string& uri, bool includeInvited) const
{
    auto repoPath = pimpl_->repoPath();
    auto invitedPath = repoPath / "invited";
    auto adminsPath = repoPath / "admins";
    auto membersPath = repoPath / "members";
    std::vector<std::filesystem::path> pathsToCheck = {adminsPath, membersPath};
    if (includeInvited)
        pathsToCheck.emplace_back(invitedPath);
    for (const auto& path : pathsToCheck) {
        for (const auto& certificate : dhtnet::fileutils::readDirectory(path)) {
            std::string_view crtUri = certificate;
            auto crtIt = crtUri.find(".crt");
            if (path != invitedPath && crtIt == std::string_view::npos) {
                JAMI_WARNING("Incorrect file found: {}/{}", path, certificate);
                continue;
            }
            if (crtIt != std::string_view::npos)
                crtUri = crtUri.substr(0, crtIt);
            if (crtUri == uri)
                return true;
        }
    }

    if (includeInvited && mode() == ConversationMode::ONE_TO_ONE) {
        for (const auto& member : getInitialMembers()) {
            if (member == uri)
                return true;
        }
    }

    return false;
}

bool
Conversation::isBanned(const std::string& uri) const
{
    return !pimpl_->bannedType(uri).empty();
}

void
Conversation::sendMessage(std::string&& message,
                          const std::string& type,
                          const std::string& replyTo,
                          OnCommitCb&& onCommit,
                          OnDoneCb&& cb)
{
    Json::Value json;
    json["body"] = std::move(message);
    json["type"] = type;
    sendMessage(std::move(json), replyTo, std::move(onCommit), std::move(cb));
}

void
Conversation::sendMessage(Json::Value&& value,
                          const std::string& replyTo,
                          OnCommitCb&& onCommit,
                          OnDoneCb&& cb)
{
    if (!replyTo.empty()) {
        auto commit = pimpl_->repository_->getCommit(replyTo);
        if (commit == std::nullopt) {
            JAMI_ERR("Replying to invalid commit %s", replyTo.c_str());
            return;
        }
        value["reply-to"] = replyTo;
    }
    dht::ThreadPool::io().run(
        [w = weak(), value = std::move(value), onCommit = std::move(onCommit), cb = std::move(cb)] {
            if (auto sthis = w.lock()) {
                std::unique_lock lk(sthis->pimpl_->writeMtx_);
                auto commit = sthis->pimpl_->repository_->commitMessage(
                    Json::writeString(jsonBuilder, value));
                lk.unlock();
                if (onCommit)
                    onCommit(commit);
                sthis->pimpl_->announce(commit, true);
                if (cb)
                    cb(!commit.empty(), commit);
            }
        });
}

void
Conversation::sendMessages(std::vector<Json::Value>&& messages, OnMultiDoneCb&& cb)
{
    dht::ThreadPool::io().run([w = weak(), messages = std::move(messages), cb = std::move(cb)] {
        if (auto sthis = w.lock()) {
            std::vector<std::string> commits;
            commits.reserve(messages.size());
            std::unique_lock lk(sthis->pimpl_->writeMtx_);
            for (const auto& message : messages) {
                auto commit = sthis->pimpl_->repository_->commitMessage(
                    Json::writeString(jsonBuilder, message));
                commits.emplace_back(std::move(commit));
            }
            lk.unlock();
            sthis->pimpl_->announce(commits, true);
            if (cb)
                cb(commits);
        }
    });
}

std::optional<std::map<std::string, std::string>>
Conversation::getCommit(const std::string& commitId) const
{
    auto commit = pimpl_->repository_->getCommit(commitId);
    if (commit == std::nullopt)
        return std::nullopt;
    return pimpl_->repository_->convCommitToMap(*commit);
}

void
Conversation::loadMessages(OnLoadMessages cb, const LogOptions& options)
{
    if (!cb)
        return;
    dht::ThreadPool::io().run([w = weak(), cb = std::move(cb), options] {
        if (auto sthis = w.lock()) {
            cb(sthis->pimpl_->loadMessages(options));
        }
    });
}

void
Conversation::loadMessages2(const OnLoadMessages2& cb, const LogOptions& options)
{
    if (!cb)
        return;
    dht::ThreadPool::io().run([w = weak(), cb = std::move(cb), options] {
        if (auto sthis = w.lock()) {
            std::lock_guard lk(sthis->pimpl_->loadedHistory_.mutex);
            cb(sthis->pimpl_->loadMessages2(options));
        }
    });
}

void
Conversation::clearCache()
{
    std::lock_guard lk(pimpl_->loadedHistory_.mutex);
    pimpl_->loadedHistory_.messageList.clear();
    pimpl_->loadedHistory_.quickAccess.clear();
    pimpl_->loadedHistory_.pendingEditions.clear();
    pimpl_->loadedHistory_.pendingReactions.clear();
    {
        std::lock_guard lk(pimpl_->messageStatusMtx_);
        pimpl_->memberToStatus.clear();
    }
}

std::string
Conversation::lastCommitId() const
{
    {
        std::lock_guard lk(pimpl_->loadedHistory_.mutex);
        if (!pimpl_->loadedHistory_.messageList.empty())
            return (*pimpl_->loadedHistory_.messageList.begin())->id;
    }
    LogOptions options;
    options.nbOfCommits = 1;
    options.skipMerge = true;
    History optHistory;
    std::scoped_lock lock(pimpl_->writeMtx_, optHistory.mutex);
    auto res = pimpl_->loadMessages2(options, &optHistory);
    if (res.empty())
        return {};
    return (*optHistory.messageList.begin())->id;
}

std::vector<std::map<std::string, std::string>>
Conversation::Impl::mergeHistory(const std::string& uri)
{
    if (not repository_) {
        JAMI_WARN("Invalid repo. Abort merge");
        return {};
    }
    auto remoteHead = repository_->remoteHead(uri);
    if (remoteHead.empty()) {
        JAMI_WARN("Unable to get HEAD of %s", uri.c_str());
        return {};
    }

    // Validate commit
    auto [newCommits, err] = repository_->validFetch(uri);
    if (newCommits.empty()) {
        if (err)
            JAMI_ERR("Unable to validate history with %s", uri.c_str());
        repository_->removeBranchWith(uri);
        return {};
    }

    // If validated, merge
    auto [ok, cid] = repository_->merge(remoteHead);
    if (!ok) {
        JAMI_ERR("Unable to merge history with %s", uri.c_str());
        repository_->removeBranchWith(uri);
        return {};
    }
    if (!cid.empty()) {
        // A merge commit was generated, should be added in new commits
        auto commit = repository_->getCommit(cid);
        if (commit != std::nullopt)
            newCommits.emplace_back(*commit);
    }

    JAMI_DEBUG("Successfully merge history with {:s}", uri);
    auto result = repository_->convCommitsToMap(newCommits);
    for (auto& commit : result) {
        auto it = commit.find("type");
        if (it != commit.end() && it->second == "member") {
            repository_->refreshMembers();

            if (commit["action"] == "ban")
                disconnectFromPeer(commit["uri"]);
        }
    }
    return result;
}

bool
Conversation::pull(const std::string& deviceId, OnPullCb&& cb, std::string commitId)
{
    std::lock_guard lk(pimpl_->pullcbsMtx_);
    auto [it, notInProgress] = pimpl_->fetchingRemotes_.emplace(deviceId, std::deque<std::pair<std::string, OnPullCb>>());
    auto& pullcbs = it->second;
    auto itPull = std::find_if(pullcbs.begin(),
                               pullcbs.end(),
                               [&](const auto& elem) { return std::get<0>(elem) == commitId; });
    if (itPull != pullcbs.end()) {
        JAMI_DEBUG("Ignoring request to pull from {:s} with commit {:s}: pull already in progress", deviceId, commitId);
        cb(false);
        return false;
    }
    JAMI_DEBUG("Pulling from {:s} with commit {:s}", deviceId, commitId);
    pullcbs.emplace_back(std::move(commitId), std::move(cb));
    if (notInProgress)
        dht::ThreadPool::io().run([w = weak(), deviceId] {
            if (auto sthis_ = w.lock())
                sthis_->pimpl_->pull(deviceId);
        });
    return true;
}

void
Conversation::Impl::pull(const std::string& deviceId)
{
    auto& repo = repository_;

    std::string commitId;
    OnPullCb cb;
    while (true) {
        {
            std::lock_guard lk(pullcbsMtx_);
            auto it = fetchingRemotes_.find(deviceId);
            if (it == fetchingRemotes_.end()) {
                JAMI_ERROR("Could not find device {:s} in fetchingRemotes", deviceId);
                break;
            }
            auto& pullcbs = it->second;
            if (pullcbs.empty()) {
                fetchingRemotes_.erase(it);
                break;
            }
            auto& elem = pullcbs.front();
            commitId = std::move(std::get<0>(elem));
            cb = std::move(std::get<1>(elem));
            pullcbs.pop_front();
        }
        // If recently fetched, the commit can already be there, so no need to do complex operations
        if (commitId != "" && repo->getCommit(commitId, false) != std::nullopt) {
            cb(true);
            continue;
        }
        // Pull from remote
        auto fetched = repo->fetch(deviceId);
        if (!fetched) {
            cb(false);
            continue;
        }
        auto oldHead = repo->getHead();
        std::string newHead = oldHead;
        std::unique_lock lk(writeMtx_);
        auto commits = mergeHistory(deviceId);
        if (!commits.empty()) {
            newHead = commits.rbegin()->at("id");
            // Note: Because clients needs to linearize the history, they need to know all commits
            // that can be updated.
            // In this case, all commits until the common merge base should be announced.
            // The client ill need to update it's model after this.
            std::string mergeBase = oldHead; // If fast-forward, the merge base is the previous head
            auto newHeadCommit = repo->getCommit(newHead);
            if (newHeadCommit != std::nullopt && newHeadCommit->parents.size() > 1) {
                mergeBase = repo->mergeBase(newHeadCommit->parents[0], newHeadCommit->parents[1]);
                LogOptions options;
                options.to = mergeBase;
                auto updatedCommits = loadMessages(options);
                // We announce commits from oldest to update to newest. This generally avoid
                // to get detached commits until they are all announced.
                std::reverse(std::begin(updatedCommits), std::end(updatedCommits));
                announce(updatedCommits);
            } else {
                announce(commits);
            }
        }
        lk.unlock();

        bool commitFound = false;
        if (commitId != "") {
            // If `commitId` is non-empty, then we were attempting to pull a specific commit.
            // We need to check if we actually got it; the fact that the fetch above was
            // successful doesn't guarantee that we did.
            for (const auto& commit : commits) {
                if (commit.at("id") == commitId) {
                    commitFound = true;
                    break;
                }
            }
        } else {
            commitFound = true;
        }
        if (!commitFound)
            JAMI_WARNING("Successfully fetched from device {} but didn't receive expected commit {}",
                         deviceId, commitId);
        // WARNING: If its argument is `true`, this callback will attempt to send a message notification
        //          for commit `commitId` to other members of the swarm. It's important that we only
        //          send these notifications if we actually have the commit. Otherwise, we can end up
        //          in a situation where the members of the swarm keep sending notifications to each
        //          other for a commit that none of them have (note that we are unable to rule this out, as
        //          nothing prevents a malicious user from intentionally sending a notification with
        //          a fake commit ID).
        if (cb)
            cb(commitFound);
        // Announce if profile changed
        if (oldHead != newHead) {
            auto diffStats = repo->diffStats(newHead, oldHead);
            auto changedFiles = repo->changedFiles(diffStats);
            if (find(changedFiles.begin(), changedFiles.end(), "profile.vcf")
                != changedFiles.end()) {
                emitSignal<libjami::ConversationSignal::ConversationProfileUpdated>(
                    accountId_, repo->id(), repo->infos());
            }
        }
    }
}

void
Conversation::sync(const std::string& member,
                   const std::string& deviceId,
                   OnPullCb&& cb,
                   std::string commitId)
{
    pull(deviceId, std::move(cb), commitId);
    dht::ThreadPool::io().run([member, deviceId, w = weak_from_this()] {
        auto sthis = w.lock();
        // For waiting request, downloadFile
        for (const auto& wr : sthis->dataTransfer()->waitingRequests()) {
            auto path = fileutils::get_data_dir() / sthis->pimpl_->accountId_
                        / "conversation_data" / sthis->id() / wr.fileId;
            auto start = fileutils::size(path);
            if (start < 0)
                start = 0;
            sthis->downloadFile(wr.interactionId, wr.fileId, wr.path, member, deviceId, start);
        }
    });
}

std::map<std::string, std::string>
Conversation::generateInvitation() const
{
    // Invite the new member to the conversation
    Json::Value root;
    auto& metadata = root[ConversationMapKeys::METADATAS];
    for (const auto& [k, v] : infos()) {
        if (v.size() >= 64000) {
            JAMI_WARNING("Cutting invite because the SIP message will be too long");
            continue;
        }
        metadata[k] = v;
    }
    root[ConversationMapKeys::CONVERSATIONID] = id();
    return {{"application/invite+json", Json::writeString(jsonBuilder, root)}};
}

std::string
Conversation::leave()
{
    setRemovingFlag();
    std::lock_guard lk(pimpl_->writeMtx_);
    return pimpl_->repository_->leave();
}

void
Conversation::setRemovingFlag()
{
    pimpl_->isRemoving_ = true;
}

bool
Conversation::isRemoving()
{
    return pimpl_->isRemoving_;
}

void
Conversation::erase()
{
    if (pimpl_->conversationDataPath_ != "")
        dhtnet::fileutils::removeAll(pimpl_->conversationDataPath_, true);
    if (!pimpl_->repository_)
        return;
    std::lock_guard lk(pimpl_->writeMtx_);
    pimpl_->repository_->erase();
}

ConversationMode
Conversation::mode() const
{
    return pimpl_->repository_->mode();
}

std::vector<std::string>
Conversation::getInitialMembers() const
{
    return pimpl_->repository_->getInitialMembers();
}

bool
Conversation::isInitialMember(const std::string& uri) const
{
    auto members = getInitialMembers();
    return std::find(members.begin(), members.end(), uri) != members.end();
}

void
Conversation::updateInfos(const std::map<std::string, std::string>& map, const OnDoneCb& cb)
{
    dht::ThreadPool::io().run([w = weak(), map = std::move(map), cb = std::move(cb)] {
        if (auto sthis = w.lock()) {
            auto& repo = sthis->pimpl_->repository_;
            std::unique_lock lk(sthis->pimpl_->writeMtx_);
            auto commit = repo->updateInfos(map);
            sthis->pimpl_->announce(commit, true);
            lk.unlock();
            if (cb)
                cb(!commit.empty(), commit);
            emitSignal<libjami::ConversationSignal::ConversationProfileUpdated>(
                sthis->pimpl_->accountId_, repo->id(), repo->infos());
        }
    });
}

std::map<std::string, std::string>
Conversation::infos() const
{
    return pimpl_->repository_->infos();
}

void
Conversation::updatePreferences(const std::map<std::string, std::string>& map)
{
    auto filePath = pimpl_->conversationDataPath_ / "preferences";
    auto prefs = map;
    auto itLast = prefs.find(LAST_MODIFIED);
    if (itLast != prefs.end()) {
        if (std::filesystem::is_regular_file(filePath)) {
            auto lastModified = fileutils::lastWriteTimeInSeconds(filePath);
            try {
                if (lastModified >= std::stoul(itLast->second))
                    return;
            } catch (...) {
                return;
            }
        }
        prefs.erase(itLast);
    }

    std::ofstream file(filePath, std::ios::trunc | std::ios::binary);
    msgpack::pack(file, prefs);
    emitSignal<libjami::ConversationSignal::ConversationPreferencesUpdated>(pimpl_->accountId_,
                                                                            id(),
                                                                            std::move(prefs));
}

std::map<std::string, std::string>
Conversation::preferences(bool includeLastModified) const
{
    try {
        std::map<std::string, std::string> preferences;
        auto filePath = pimpl_->conversationDataPath_ / "preferences";
        auto file = fileutils::loadFile(filePath);
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
        oh.get().convert(preferences);
        if (includeLastModified)
            preferences[LAST_MODIFIED] = std::to_string(fileutils::lastWriteTimeInSeconds(filePath));
        return preferences;
    } catch (const std::exception& e) {
    }
    return {};
}

std::vector<uint8_t>
Conversation::vCard() const
{
    try {
        return fileutils::loadFile(pimpl_->repoPath() / "profile.vcf");
    } catch (...) {
    }
    return {};
}

std::shared_ptr<TransferManager>
Conversation::dataTransfer() const
{
    return pimpl_->transferManager_;
}

bool
Conversation::onFileChannelRequest(const std::string& member,
                                   const std::string& fileId,
                                   std::filesystem::path& path,
                                   std::string& sha3sum) const
{
    if (!isMember(member))
        return false;

    auto sep = fileId.find('_');
    if (sep == std::string::npos)
        return false;

    auto interactionId = fileId.substr(0, sep);
    auto commit = getCommit(interactionId);
    if (commit == std::nullopt || commit->find("type") == commit->end()
        || commit->find("tid") == commit->end() || commit->find("sha3sum") == commit->end()
        || commit->at("type") != "application/data-transfer+json") {
        JAMI_WARNING("[Account {:s}] {} requested invalid file transfer commit {}",
                     pimpl_->accountId_,
                     member,
                     interactionId);
        return false;
    }

    path = dataTransfer()->path(fileId);
    sha3sum = commit->at("sha3sum");

    return true;
}

bool
Conversation::downloadFile(const std::string& interactionId,
                           const std::string& fileId,
                           const std::string& path,
                           const std::string&,
                           const std::string& deviceId,
                           std::size_t start,
                           std::size_t end)
{
    auto commit = getCommit(interactionId);
    if (commit == std::nullopt || commit->at("type") != "application/data-transfer+json") {
        JAMI_ERROR("Commit doesn't exists or is not a file transfer {} (Conversation: {}) ", interactionId, id());
        return false;
    }
    auto tid = commit->find("tid");
    auto sha3sum = commit->find("sha3sum");
    auto size_str = commit->find("totalSize");

    if (tid == commit->end() || sha3sum == commit->end() || size_str == commit->end()) {
        JAMI_ERROR("Invalid file transfer commit (missing tid, size or sha3)");
        return false;
    }

    auto totalSize = to_int<ssize_t>(size_str->second, (ssize_t) -1);
    if (totalSize < 0) {
        JAMI_ERROR("Invalid file size {}", totalSize);
        return false;
    }

    // Be sure to not lock conversation
    dht::ThreadPool().io().run([w = weak(),
                                deviceId,
                                fileId,
                                interactionId,
                                sha3sum = sha3sum->second,
                                path,
                                totalSize,
                                start,
                                end] {
        if (auto shared = w.lock()) {
            auto acc = shared->pimpl_->account_.lock();
            if (!acc)
                return;
            shared->dataTransfer()->waitForTransfer(fileId, interactionId, sha3sum, path, totalSize);
            acc->askForFileChannel(shared->id(), deviceId, interactionId, fileId, start, end);
        }
    });
    return true;
}

void
Conversation::hasFetched(const std::string& deviceId, const std::string& commitId)
{
    dht::ThreadPool::io().run([w = weak(), deviceId, commitId]() {
        auto sthis = w.lock();
        if (!sthis)
            return;
        // Update fetched for Uri
        auto uri = sthis->uriFromDevice(deviceId);
        if (uri.empty() || uri == sthis->pimpl_->userId_)
            return;
        // When a user fetches a commit, the message is sent for this person
        sthis->pimpl_->updateStatus(uri, libjami::Account::MessageStates::SENT, commitId, std::to_string(std::time(nullptr)), true);
    });
}


void
Conversation::Impl::updateStatus(const std::string& uri,
                      libjami::Account::MessageStates st,
                      const std::string& commitId,
                      const std::string& ts,
                      bool emit)
{
    // This method can be called if peer send us a status or if another device sync. Emit will be true if a peer send us a status and will emit to other connected devices.
    LogOptions options;
    std::map<std::string, std::map<std::string, std::string>> newStatus;
    {
        // Update internal structures.
        std::lock_guard lk(messageStatusMtx_);
        auto& status = messagesStatus_[uri];
        auto& oldStatus = status[st == libjami::Account::MessageStates::SENT ? "fetched" : "read"];
        if (oldStatus == commitId)
            return; // Nothing to do
        options.to = oldStatus;
        options.from = commitId;
        oldStatus = commitId;
        status[st == libjami::Account::MessageStates::SENT ? "fetched_ts" : "read_ts"] = ts;
        saveStatus();
        if (emit)
            newStatus[uri].insert(status.begin(), status.end());
    }
    if (emit && messageStatusCb_) {
        messageStatusCb_(newStatus);
    }
    // Update messages status for all commit between the old and new one
    options.logIfNotFound = false;
    options.fastLog = true;
    History optHistory;
    std::lock_guard lk(optHistory.mutex); // Avoid to announce messages while updating status.
    auto res = loadMessages2(options, &optHistory);
    if (res.size() == 0) {
        // In this case, commit is not received yet, so we cache it
        futureStatus[commitId][uri] = static_cast<int32_t>(st);
    }
    for (const auto& [cid, _]: optHistory.quickAccess) {
        auto message = loadedHistory_.quickAccess.find(cid);
        if (message != loadedHistory_.quickAccess.end()) {
            // Update message and emit to client,
            if(static_cast<int32_t>(st) > message->second->status[uri]){
                message->second->status[uri] = static_cast<int32_t>(st);
                emitSignal<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
                    accountId_,
                    repository_->id(),
                    uri,
                    cid,
                    static_cast<int>(st));
            }
        } else {
            // In this case, commit is not loaded by client, so we cache it
            // No need to emit to client, they will get a correct status on load.
            futureStatus[cid][uri] = static_cast<int32_t>(st);
        }
    }
}

bool
Conversation::setMessageDisplayed(const std::string& uri, const std::string& interactionId)
{
    std::lock_guard lk(pimpl_->messageStatusMtx_);
    if (pimpl_->messagesStatus_[uri]["read"] == interactionId)
        return false; // Nothing to do
    dht::ThreadPool::io().run([w = weak(), uri, interactionId]() {
        auto sthis = w.lock();
        if (!sthis)
            return;
        sthis->pimpl_->updateStatus(uri, libjami::Account::MessageStates::DISPLAYED, interactionId, std::to_string(std::time(nullptr)), true);
    });
    return true;
}

std::map<std::string, std::map<std::string, std::string>>
Conversation::messageStatus() const
{
    std::lock_guard lk(pimpl_->messageStatusMtx_);
    return pimpl_->messagesStatus_;
}

void
Conversation::updateMessageStatus(const std::map<std::string, std::map<std::string, std::string>>& messageStatus)
{
    std::unique_lock lk(pimpl_->messageStatusMtx_);
    std::vector<std::tuple<libjami::Account::MessageStates, std::string, std::string, std::string>> stVec;
    for (const auto& [uri, status] : messageStatus) {
        auto& oldMs = pimpl_->messagesStatus_[uri];
        if (status.find("fetched_ts") != status.end() && status.at("fetched") != oldMs["fetched"]) {
            if (oldMs["fetched_ts"].empty() || std::stol(oldMs["fetched_ts"]) <= std::stol(status.at("fetched_ts"))) {
                stVec.emplace_back(libjami::Account::MessageStates::SENT, uri, status.at("fetched"), status.at("fetched_ts"));
            }
        }
        if (status.find("read_ts") != status.end() && status.at("read") != oldMs["read"]) {
            if (oldMs["read_ts"].empty() || std::stol(oldMs["read_ts"]) <= std::stol(status.at("read_ts"))) {
                stVec.emplace_back(libjami::Account::MessageStates::DISPLAYED, uri, status.at("read"), status.at("read_ts"));
            }
        }
    }
    lk.unlock();

    for (const auto& [status, uri, commitId, ts] : stVec) {
        pimpl_->updateStatus(uri, status, commitId, ts);
    }
}

void
Conversation::onMessageStatusChanged(const std::function<void(const std::map<std::string, std::map<std::string, std::string>>&)>& cb)
{
    std::unique_lock lk(pimpl_->messageStatusMtx_);
    pimpl_->messageStatusCb_ = cb;
}

#ifdef LIBJAMI_TESTABLE
void
Conversation::onBootstrapStatus(const std::function<void(std::string, BootstrapStatus)>& cb)
{
    pimpl_->bootstrapCbTest_ = cb;
}
#endif

void
Conversation::checkBootstrapMember(const asio::error_code& ec,
                                   std::vector<std::map<std::string, std::string>> members)
{
    if (ec == asio::error::operation_aborted)
        return;
    auto acc = pimpl_->account_.lock();
    if (pimpl_->swarmManager_->getRoutingTable().getNodes().size() > 0 or not acc)
        return;
    // We bootstrap the DRT with devices who already wrote in the repository.
    // However, in a conversation, a large number of devices may just watch
    // the conversation, but never write any message.
    std::unique_lock lock(pimpl_->membersMtx_);

    std::string uri;
    while (!members.empty()) {
        auto member = std::move(members.back());
        members.pop_back();
        uri = std::move(member.at("uri"));
        if (uri != pimpl_->userId_
            && pimpl_->checkedMembers_.find(uri) == pimpl_->checkedMembers_.end())
            break;
    }
    auto fallbackFailed = [](auto sthis) {
        JAMI_WARNING("{}[SwarmManager {}] Bootstrap: Fallback failed. Wait for remote connections.",
                    sthis->pimpl_->toString(),
                    fmt::ptr(sthis->pimpl_->swarmManager_.get()));
#ifdef LIBJAMI_TESTABLE
        if (sthis->pimpl_->bootstrapCbTest_)
            sthis->pimpl_->bootstrapCbTest_(sthis->id(), BootstrapStatus::FAILED);
#endif
    };
    // If members is empty, we finished the fallback un-successfully
    if (members.empty() && uri.empty()) {
        lock.unlock();
        fallbackFailed(this);
        return;
    }

    // Fallback, check devices of a member (we didn't check yet) in the conversation
    pimpl_->checkedMembers_.emplace(uri);
    auto devices = std::make_shared<std::vector<NodeId>>();
    acc->forEachDevice(
        dht::InfoHash(uri),
        [w = weak(), devices](const std::shared_ptr<dht::crypto::PublicKey>& dev) {
            // Test if already sent
            if (auto sthis = w.lock()) {
                if (!sthis->pimpl_->swarmManager_->getRoutingTable().hasKnownNode(dev->getLongId()))
                    devices->emplace_back(dev->getLongId());
            }
        },
        [w = weak(), devices, members = std::move(members), uri, fallbackFailed=std::move(fallbackFailed)](bool ok) {
            auto sthis = w.lock();
            if (!sthis)
                return;
            auto checkNext = true;
            if (ok && devices->size() != 0) {
#ifdef LIBJAMI_TESTABLE
                if (sthis->pimpl_->bootstrapCbTest_)
                    sthis->pimpl_->bootstrapCbTest_(sthis->id(), BootstrapStatus::FALLBACK);
#endif
                JAMI_WARNING("{}[SwarmManager {}] Bootstrap: Fallback with member: {}",
                             sthis->pimpl_->toString(),
                             fmt::ptr(sthis->pimpl_->swarmManager_),
                             uri);
                if (sthis->pimpl_->swarmManager_->setKnownNodes(*devices))
                    checkNext = false;
            }
            if (checkNext) {
                // Check next member
                sthis->pimpl_->fallbackTimer_->expires_at(std::chrono::steady_clock::now());
                sthis->pimpl_->fallbackTimer_->async_wait(
                    std::bind(&Conversation::checkBootstrapMember,
                              sthis,
                              std::placeholders::_1,
                              std::move(members)));
            } else {
                // In this case, all members are checked. Fallback failed
                fallbackFailed(sthis);
            }
        });
}

void
Conversation::bootstrap(std::function<void()> onBootstraped,
                        const std::vector<DeviceId>& knownDevices)
{
    if (!pimpl_ || !pimpl_->repository_ || !pimpl_->swarmManager_)
        return;
    // Here, we bootstrap the DRT with devices who already wrote in the conversation
    // If this doesn't work, it will attempt to fallback with checkBootstrapMember
    // If it works, the callback onConnectionChanged will be called with ok=true
    pimpl_->bootstrapCb_ = std::move(onBootstraped);
    std::vector<DeviceId> devices = knownDevices;
    for (const auto& [member, memberDevices] : pimpl_->repository_->devices()) {
        if (!isBanned(member))
            devices.insert(devices.end(), memberDevices.begin(), memberDevices.end());
    }
    JAMI_DEBUG("{}[SwarmManager {}] Bootstrap with {} device(s)",
               pimpl_->toString(),
               fmt::ptr(pimpl_->swarmManager_),
               devices.size());
    // set callback
    auto fallback = [](auto sthis, bool now = false) {
        // Fallback
        auto acc = sthis->pimpl_->account_.lock();
        if (!acc)
            return;
        auto members = sthis->getMembers(false, false);
        std::shuffle(members.begin(), members.end(), acc->rand);
        if (now) {
            sthis->pimpl_->fallbackTimer_->expires_at(std::chrono::steady_clock::now());
        } else {
            auto timeForBootstrap = std::min(static_cast<size_t>(8), members.size());
            sthis->pimpl_->fallbackTimer_->expires_at(std::chrono::steady_clock::now() + 20s
                                                        - std::chrono::seconds(timeForBootstrap));
            JAMI_DEBUG("{}[SwarmManager {}] Fallback in {} seconds",
                        sthis->pimpl_->toString(),
                        fmt::ptr(sthis->pimpl_->swarmManager_.get()),
                        (20 - timeForBootstrap));
        }
        sthis->pimpl_->fallbackTimer_->async_wait(std::bind(&Conversation::checkBootstrapMember,
                                                            sthis,
                                                            std::placeholders::_1,
                                                            std::move(members)));
    };

    pimpl_->swarmManager_->onConnectionChanged([w = weak(), fallback](bool ok) {
        // This will call methods from accounts, so trigger on another thread.
        dht::ThreadPool::io().run([w, ok, fallback=std::move(fallback)] {
            auto sthis = w.lock();
            if (!sthis)
                return;
            if (ok) {
                // Bootstrap succeeded!
                {
                    std::lock_guard lock(sthis->pimpl_->membersMtx_);
                    sthis->pimpl_->checkedMembers_.clear();
                }
                if (sthis->pimpl_->bootstrapCb_)
                    sthis->pimpl_->bootstrapCb_();
#ifdef LIBJAMI_TESTABLE
                if (sthis->pimpl_->bootstrapCbTest_)
                    sthis->pimpl_->bootstrapCbTest_(sthis->id(), BootstrapStatus::SUCCESS);
#endif
                return;
            }
            fallback(sthis);
        });
    });
    {
        std::lock_guard lock(pimpl_->membersMtx_);
        pimpl_->checkedMembers_.clear();
    }
    // If is shutdown, the conversation was re-added, causing no new nodes to be connected, but just a classic connectivity change
    if (pimpl_->swarmManager_->isShutdown()) {
        pimpl_->swarmManager_->restart();
        pimpl_->swarmManager_->maintainBuckets();
    } else if (!pimpl_->swarmManager_->setKnownNodes(devices)) {
        fallback(this, true);
    }
}

std::vector<std::string>
Conversation::commitsEndedCalls()
{
    pimpl_->loadActiveCalls();
    pimpl_->loadHostedCalls();
    auto commits = pimpl_->commitsEndedCalls();
    if (!commits.empty()) {
        // Announce to client
        dht::ThreadPool::io().run([w = weak(), commits] {
            if (auto sthis = w.lock())
                sthis->pimpl_->announce(commits, true);
        });
    }
    return commits;
}

void
Conversation::onMembersChanged(OnMembersChanged&& cb)
{
    pimpl_->onMembersChanged_ = std::move(cb);
    pimpl_->repository_->onMembersChanged([w=weak()] (const std::set<std::string>& memberUris) {
        if (auto sthis = w.lock())
            sthis->pimpl_->onMembersChanged_(memberUris);
    });
}

void
Conversation::onNeedSocket(NeedSocketCb needSocket)
{
    pimpl_->swarmManager_->needSocketCb_ = [needSocket = std::move(needSocket),
                                            w=weak()](const std::string& deviceId, ChannelCb&& cb) {
        if (auto sthis = w.lock())
            needSocket(sthis->id(), deviceId, std::move(cb), "application/im-gitmessage-id");
    };
}

void
Conversation::addSwarmChannel(std::shared_ptr<dhtnet::ChannelSocket> channel)
{
    auto deviceId = channel->deviceId();
    // Transmit avatar if necessary
    // We do this here, because at this point we know both sides are connected and in
    // the same conversation
    // addSwarmChannel is a bit more complex, but it should be the best moment to do this.
    auto cert = channel->peerCertificate();
    if (!cert || !cert->issuer)
        return;
    auto member = cert->issuer->getId().toString();
    pimpl_->swarmManager_->addChannel(std::move(channel));
    dht::ThreadPool::io().run([member, deviceId, a = pimpl_->account_, w = weak_from_this()] {
        auto sthis = w.lock();
        if (auto account = a.lock()) {
            account->sendProfile(sthis->id(), member, deviceId.toString());
        }
    });
}

uint32_t
Conversation::countInteractions(const std::string& toId,
                                const std::string& fromId,
                                const std::string& authorUri) const
{
    LogOptions options;
    options.to = toId;
    options.from = fromId;
    options.authorUri = authorUri;
    options.logIfNotFound = false;
    options.fastLog = true;
    History history;
    std::lock_guard lk(history.mutex);
    auto res = pimpl_->loadMessages2(options, &history);
    return res.size();
}

void
Conversation::search(uint32_t req,
                     const Filter& filter,
                     const std::shared_ptr<std::atomic_int>& flag) const
{
    // Because logging a conversation can take quite some time,
    // do it asynchronously
    dht::ThreadPool::io().run([w = weak(), req, filter, flag] {
        if (auto sthis = w.lock()) {
            History history;
            std::vector<std::map<std::string, std::string>> commits {};
            // std::regex_constants::ECMAScript is the default flag.
            auto re = std::regex(filter.regexSearch,
                                 filter.caseSensitive ? std::regex_constants::ECMAScript
                                                      : std::regex_constants::icase);
            sthis->pimpl_->repository_->log(
                [&](const auto& id, const auto& author, auto& commit) {
                    if (!filter.author.empty()
                        && filter.author != sthis->uriFromDevice(author.email)) {
                        // Filter author
                        return CallbackResult::Skip;
                    }
                    auto commitTime = git_commit_time(commit.get());
                    if (filter.before && filter.before < commitTime) {
                        // Only get commits before this date
                        return CallbackResult::Skip;
                    }
                    if (filter.after && filter.after > commitTime) {
                        // Only get commits before this date
                        if (git_commit_parentcount(commit.get()) <= 1)
                            return CallbackResult::Break;
                        else
                            return CallbackResult::Skip; // Because we are sorting it with
                                                         // GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME
                    }

                    return CallbackResult::Ok; // Continue
                },
                [&](auto&& cc) {
                    if (auto optMessage = sthis->pimpl_->repository_->convCommitToMap(cc))
                        sthis->pimpl_->addToHistory(history, {optMessage.value()}, false, false);
                },
                [&](auto id, auto, auto) {
                    if (id == filter.lastId)
                        return true;
                    return false;
                },
                "",
                false);
            // Search on generated history
            for (auto& message : history.messageList) {
                auto contentType = message->type;
                auto isSearchable = contentType == "text/plain"
                                    || contentType == "application/data-transfer+json";
                if (filter.type.empty() && !isSearchable) {
                    // Not searchable, at least for now
                    continue;
                } else if (contentType == filter.type || filter.type.empty()) {
                    if (isSearchable) {
                        // If it's a text match the body, else the display name
                        auto body = contentType == "text/plain" ? message->body.at("body")
                                                                : message->body.at("displayName");
                        std::smatch body_match;
                        if (std::regex_search(body, body_match, re)) {
                            auto commit = message->body;
                            commit["id"] = message->id;
                            commit["type"] = message->type;
                            commits.emplace_back(commit);
                        }
                    } else {
                        // Matching type, just add it to the results
                        commits.emplace_back(message->body);
                    }

                    if (filter.maxResult != 0 && commits.size() == filter.maxResult)
                        break;
                }
            }

            if (commits.size() > 0)
                emitSignal<libjami::ConversationSignal::MessagesFound>(req,
                                                                       sthis->pimpl_->accountId_,
                                                                       sthis->id(),
                                                                       std::move(commits));
            // If we're the latest thread, inform client that the search is finished
            if ((*flag)-- == 1 /* decrement return the old value */) {
                emitSignal<libjami::ConversationSignal::MessagesFound>(
                    req,
                    sthis->pimpl_->accountId_,
                    std::string {},
                    std::vector<std::map<std::string, std::string>> {});
            }
        }
    });
}

void
Conversation::hostConference(Json::Value&& message, OnDoneCb&& cb)
{
    if (!message.isMember("confId")) {
        JAMI_ERR() << "Malformed commit";
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto nowSecs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    {
        std::lock_guard lk(pimpl_->activeCallsMtx_);
        pimpl_->hostedCalls_[message["confId"].asString()] = nowSecs;
        pimpl_->saveHostedCalls();
    }

    sendMessage(std::move(message), "", {}, std::move(cb));
}

bool
Conversation::isHosting(const std::string& confId) const
{
    auto info = infos();
    if (info["rdvDevice"] == pimpl_->deviceId_ && info["rdvHost"] == pimpl_->userId_)
        return true; // We are the current device Host
    std::lock_guard lk(pimpl_->activeCallsMtx_);
    return pimpl_->hostedCalls_.find(confId) != pimpl_->hostedCalls_.end();
}

void
Conversation::removeActiveConference(Json::Value&& message, OnDoneCb&& cb)
{
    if (!message.isMember("confId")) {
        JAMI_ERR() << "Malformed commit";
        return;
    }

    auto erased = false;
    {
        std::lock_guard lk(pimpl_->activeCallsMtx_);
        erased = pimpl_->hostedCalls_.erase(message["confId"].asString());
    }
    if (erased) {
        pimpl_->saveHostedCalls();
        sendMessage(std::move(message), "", {}, std::move(cb));
    } else
        cb(false, "");
}

std::vector<std::map<std::string, std::string>>
Conversation::currentCalls() const
{
    std::lock_guard lk(pimpl_->activeCallsMtx_);
    return pimpl_->activeCalls_;
}
} // namespace jami
