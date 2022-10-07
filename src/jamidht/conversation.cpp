/*
 *  Copyright (C) 2014-2022 Savoir-faire Linux Inc.
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

#ifdef ENABLE_PLUGIN
#include "manager.h"
#include "plugin/jamipluginmanager.h"
#include "plugin/streamdata.h"
#endif

namespace jami {

static const char* const LAST_MODIFIED = "lastModified";

ConvInfo::ConvInfo(const Json::Value& json)
{
    id = json[ConversationMapKeys::ID].asString();
    created = json[ConversationMapKeys::CREATED].asLargestUInt();
    removed = json[ConversationMapKeys::REMOVED].asLargestUInt();
    erased = json[ConversationMapKeys::ERASED].asLargestUInt();
    for (const auto& v : json[ConversationMapKeys::MEMBERS]) {
        members.emplace_back(v["uri"].asString());
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

class Conversation::Impl
{
public:
    Impl(const std::weak_ptr<JamiAccount>& account,
         ConversationMode mode,
         const std::string& otherMember = "")
        : account_(account)
    {
        repository_ = ConversationRepository::createConversation(account, mode, otherMember);
        if (!repository_) {
            throw std::logic_error("Couldn't create repository");
        }
        init();
    }

    Impl(const std::weak_ptr<JamiAccount>& account, const std::string& conversationId)
        : account_(account)
    {
        repository_ = std::make_unique<ConversationRepository>(account, conversationId);
        if (!repository_) {
            throw std::logic_error("Couldn't create repository");
        }
        init();
    }

    Impl(const std::weak_ptr<JamiAccount>& account,
         const std::string& remoteDevice,
         const std::string& conversationId)
        : account_(account)
    {
        repository_ = ConversationRepository::cloneConversation(account,
                                                                remoteDevice,
                                                                conversationId);
        if (!repository_) {
            if (auto shared = account.lock()) {
                emitSignal<DRing::ConversationSignal::OnConversationError>(
                    shared->getAccountID(), conversationId, EFETCH, "Couldn't clone repository");
            }
            throw std::logic_error("Couldn't clone repository");
        }
        init();
    }

    void init()
    {
        if (auto shared = account_.lock()) {
            accountId_ = shared->getAccountID();
            transferManager_ = std::make_shared<TransferManager>(shared->getAccountID(),
                                                                 repository_->id());
            conversationDataPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                                    + shared->getAccountID() + DIR_SEPARATOR_STR
                                    + "conversation_data" + DIR_SEPARATOR_STR + repository_->id();
            fetchedPath_ = conversationDataPath_ + DIR_SEPARATOR_STR + "fetched";
            sendingPath_ = conversationDataPath_ + DIR_SEPARATOR_STR + "sending";
            lastDisplayedPath_ = conversationDataPath_ + DIR_SEPARATOR_STR
                                 + ConversationMapKeys::LAST_DISPLAYED;
            preferencesPath_ = conversationDataPath_ + DIR_SEPARATOR_STR
                               + ConversationMapKeys::PREFERENCES;
            loadFetched();
            loadSending();
            loadLastDisplayed();
        }
    }

    ~Impl() = default;

    bool isAdmin() const;
    std::string repoPath() const;

    std::mutex writeMtx_ {};
    void announce(const std::string& commitId) const
    {
        std::vector<std::string> vec;
        if (!commitId.empty())
            vec.emplace_back(commitId);
        announce(vec);
    }

    void announce(const std::vector<std::string>& commits) const
    {
        std::vector<ConversationCommit> convcommits;
        convcommits.reserve(commits.size());
        for (const auto& cid : commits) {
            auto commit = repository_->getCommit(cid);
            if (commit != std::nullopt) {
                convcommits.emplace_back(*commit);
            }
        }
        announce(repository_->convCommitToMap(convcommits));
    }

    void announce(const std::vector<std::map<std::string, std::string>>& commits) const
    {
        auto shared = account_.lock();
        if (!shared or !repository_)
            return;
        auto convId = repository_->id();
        auto ok = !commits.empty();
        auto lastId = ok ? commits.rbegin()->at(ConversationMapKeys::ID) : "";
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
                        if (action != -1) {
                            announceMember = true;
                            emitSignal<DRing::ConversationSignal::ConversationMemberEvent>(accountId_,
                                                                                           convId,
                                                                                           uri,
                                                                                           action);
                        }
                    }
                }
#ifdef ENABLE_PLUGIN
                auto& pluginChatManager
                    = Manager::instance().getJamiPluginManager().getChatServicesManager();
                if (pluginChatManager.hasHandlers()) {
                    auto cm = std::make_shared<JamiMessage>(accountId_,
                                                            convId,
                                                            c.at("author") != shared->getUsername(),
                                                            c,
                                                            false);
                    cm->isSwarm = true;
                    pluginChatManager.publishMessage(std::move(cm));
                }
#endif
                // announce message
                emitSignal<DRing::ConversationSignal::MessageReceived>(accountId_, convId, c);
                // check if we should update lastDisplayed
                // ignore merge commits as it's not generated by the user
                if (c.at("type") == "merge")
                    continue;
                std::unique_lock<std::mutex> lk(lastDisplayedMtx_);
                auto cached = lastDisplayed_.find(ConversationMapKeys::CACHED);
                auto updateCached = false;
                if (cached != lastDisplayed_.end()) {
                    // If we found the commit we wait
                    if (cached->second == c.at(ConversationMapKeys::ID)) {
                        updateCached = true;
                        lastDisplayed_.erase(cached);
                    }
                } else if (c.at("author") == shared->getUsername()) {
                    updateCached = true;
                }

                if (updateCached) {
                    lastDisplayed_[shared->getUsername()] = c.at(ConversationMapKeys::ID);
                    saveLastDisplayed();
                    lk.unlock();
                    if (lastDisplayedUpdatedCb_)
                        lastDisplayedUpdatedCb_(convId, c.at(ConversationMapKeys::ID));
                }
            }

            if (announceMember) {
                std::vector<std::string> members;
                for (const auto& m : repository_->members())
                    members.emplace_back(m.uri);
                shared->saveMembers(convId, members);
            }
        }
    }

    void loadFetched()
    {
        try {
            // read file
            auto file = fileutils::loadFile(fetchedPath_);
            // load values
            msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
            std::lock_guard<std::mutex> lk {fetchedDevicesMtx_};
            oh.get().convert(fetchedDevices_);
        } catch (const std::exception& e) {
            return;
        }
    }
    void saveFetched()
    {
        std::ofstream file(fetchedPath_, std::ios::trunc | std::ios::binary);
        msgpack::pack(file, fetchedDevices_);
    }

    void loadSending()
    {
        try {
            // read file
            auto file = fileutils::loadFile(sendingPath_);
            // load values
            msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
            std::lock_guard<std::mutex> lk {writeMtx_};
            oh.get().convert(sending_);
        } catch (const std::exception& e) {
            return;
        }
    }
    void saveSending()
    {
        std::ofstream file(sendingPath_, std::ios::trunc | std::ios::binary);
        msgpack::pack(file, sending_);
    }

    void loadLastDisplayed() const
    {
        try {
            // read file
            auto file = fileutils::loadFile(lastDisplayedPath_);
            // load values
            msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
            std::lock_guard<std::mutex> lk {lastDisplayedMtx_};
            oh.get().convert(lastDisplayed_);
        } catch (const std::exception& e) {
            return;
        }
    }

    void saveLastDisplayed() const
    {
        std::ofstream file(lastDisplayedPath_, std::ios::trunc | std::ios::binary);
        msgpack::pack(file, lastDisplayed_);
    }

    void voteUnban(const std::string& contactUri, const std::string& type, const OnDoneCb& cb);

    std::string bannedType(const std::string& uri) const
    {
        auto bannedMember = fmt::format("{}/banned/members/{}.crt", repoPath(), uri);
        if (fileutils::isFile(bannedMember))
            return "members";
        auto bannedAdmin = fmt::format("{}/banned/admins/{}.crt", repoPath(), uri);
        if (fileutils::isFile(bannedAdmin))
            return "admins";
        auto bannedInvited = fmt::format("{}/banned/invited/{}", repoPath(), uri);
        if (fileutils::isFile(bannedInvited))
            return "invited";
        auto bannedDevice = fmt::format("{}/banned/devices/{}.crt", repoPath(), uri);
        if (fileutils::isFile(bannedDevice))
            return "devices";
        return {};
    }

    std::unique_ptr<ConversationRepository> repository_;
    std::weak_ptr<JamiAccount> account_;
    std::atomic_bool isRemoving_ {false};
    std::vector<std::map<std::string, std::string>> loadMessages(const std::string& fromMessage = "",
                                                                 const std::string& toMessage = "",
                                                                 size_t n = 0);
    void pull();
    std::vector<std::map<std::string, std::string>> mergeHistory(const std::string& uri);

    std::mutex pullcbsMtx_ {};
    std::set<std::string> fetchingRemotes_ {}; // store current remote in fetch
    std::deque<std::tuple<std::string, std::string, OnPullCb>> pullcbs_ {};
    std::shared_ptr<TransferManager> transferManager_ {};
    std::string conversationDataPath_ {};
    std::string fetchedPath_ {};
    std::mutex fetchedDevicesMtx_ {};
    std::set<std::string> fetchedDevices_ {};
    // Manage last message displayed and status
    std::string sendingPath_ {};
    std::vector<std::string> sending_ {};
    // Manage last message displayed
    std::string accountId_ {};
    std::string lastDisplayedPath_ {};
    std::string preferencesPath_ {};
    mutable std::mutex lastDisplayedMtx_ {}; // for lastDisplayed_
    mutable std::map<std::string, std::string> lastDisplayed_ {};
    std::function<void(const std::string&, const std::string&)> lastDisplayedUpdatedCb_ {};
};

bool
Conversation::Impl::isAdmin() const
{
    auto shared = account_.lock();
    if (!shared)
        return false;

    auto adminsPath = repoPath() + DIR_SEPARATOR_STR + "admins";
    auto cert = shared->identity().second;
    if (!cert->issuer)
        return false;
    auto uri = cert->issuer->getId().toString();
    return fileutils::isFile(fileutils::getFullPath(adminsPath, uri + ".crt"));
}

std::string
Conversation::Impl::repoPath() const
{
    return fileutils::get_data_dir() + DIR_SEPARATOR_STR + accountId_ + DIR_SEPARATOR_STR
           + "conversations" + DIR_SEPARATOR_STR + repository_->id();
}

std::vector<std::map<std::string, std::string>>
Conversation::Impl::loadMessages(const std::string& fromMessage,
                                 const std::string& toMessage,
                                 size_t n)
{
    if (!repository_)
        return {};
    std::vector<ConversationCommit> convCommits;
    if (toMessage.empty())
        convCommits = repository_->logN(fromMessage, n);
    else
        convCommits = repository_->log(fromMessage, toMessage);
    return repository_->convCommitToMap(convCommits);
}

Conversation::Conversation(const std::weak_ptr<JamiAccount>& account,
                           ConversationMode mode,
                           const std::string& otherMember)
    : pimpl_ {new Impl {account, mode, otherMember}}
{}

Conversation::Conversation(const std::weak_ptr<JamiAccount>& account,
                           const std::string& conversationId)
    : pimpl_ {new Impl {account, conversationId}}
{}

Conversation::Conversation(const std::weak_ptr<JamiAccount>& account,
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
                JAMI_WARN("Cannot add new member in one to one conversation");
                cb(false, "");
                return;
            }
        }
    } catch (const std::exception& e) {
        JAMI_WARN("Cannot get mode: %s", e.what());
        cb(false, "");
        return;
    }
    if (isMember(contactUri, true)) {
        JAMI_WARN("Could not add member %s because it's already a member", contactUri.c_str());
        cb(false, "");
        return;
    }
    if (isBanned(contactUri)) {
        if (pimpl_->isAdmin()) {
            dht::ThreadPool::io().run(
                [w = weak(), contactUri = std::move(contactUri), cb = std::move(cb)] {
                    if (auto sthis = w.lock()) {
                        auto members = sthis->pimpl_->repository_->members();
                        std::string type = sthis->pimpl_->bannedType(contactUri);
                        if (type.empty()) {
                            cb(false, {});
                            return;
                        }
                        sthis->pimpl_->voteUnban(contactUri, type, cb);
                    }
                });
        } else {
            JAMI_WARN("Could not add member %s because this member is banned", contactUri.c_str());
            cb(false, "");
        }
        return;
    }

    dht::ThreadPool::io().run([w = weak(), contactUri = std::move(contactUri), cb = std::move(cb)] {
        if (auto sthis = w.lock()) {
            // Add member files and commit
            std::unique_lock<std::mutex> lk(sthis->pimpl_->writeMtx_);
            auto commit = sthis->pimpl_->repository_->addMember(contactUri);
            sthis->pimpl_->announce(commit);
            lk.unlock();
            if (cb)
                cb(!commit.empty(), commit);
        }
    });
}

void
Conversation::Impl::voteUnban(const std::string& contactUri,
                              const std::string& type,
                              const OnDoneCb& cb)
{
    // Check if admin
    if (!isAdmin()) {
        JAMI_WARN("You're not an admin of this repo. Cannot unban %s", contactUri.c_str());
        cb(false, {});
        return;
    }

    // Vote for removal
    std::unique_lock<std::mutex> lk(writeMtx_);
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
    announce(commits);
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
                JAMI_WARN("You're not an admin of this repo. Cannot ban %s", contactUri.c_str());
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
            std::unique_lock<std::mutex> lk(sthis->pimpl_->writeMtx_);
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
            }
            sthis->pimpl_->announce(commits);
            lk.unlock();
            cb(!lastId.empty(), lastId);
        }
    });
}

std::vector<std::map<std::string, std::string>>
Conversation::getMembers(bool includeInvited, bool includeLeft) const
{
    std::vector<std::map<std::string, std::string>> result;
    auto members = pimpl_->repository_->members();
    std::lock_guard<std::mutex> lk(pimpl_->lastDisplayedMtx_);
    for (const auto& member : members) {
        if (member.role == MemberRole::BANNED)
            continue;
        if (member.role == MemberRole::INVITED && !includeInvited)
            continue;
        if (member.role == MemberRole::LEFT && !includeLeft)
            continue;
        auto mm = member.map();
        std::string lastDisplayed;
        auto itDisplayed = pimpl_->lastDisplayed_.find(member.uri);
        if (itDisplayed != pimpl_->lastDisplayed_.end()) {
            lastDisplayed = itDisplayed->second;
        }
        mm[ConversationMapKeys::LAST_DISPLAYED] = std::move(lastDisplayed);
        result.emplace_back(std::move(mm));
    }
    return result;
}

std::vector<std::string>
Conversation::memberUris(std::string_view filter, const std::set<MemberRole>& filteredRoles) const
{
    return pimpl_->repository_->memberUris(filter, filteredRoles);
}

std::string
Conversation::join()
{
    return pimpl_->repository_->join();
}

bool
Conversation::isMember(const std::string& uri, bool includeInvited) const
{
    auto invitedPath = pimpl_->repoPath() + DIR_SEPARATOR_STR + "invited";
    auto adminsPath = pimpl_->repoPath() + DIR_SEPARATOR_STR + "admins";
    auto membersPath = pimpl_->repoPath() + DIR_SEPARATOR_STR + "members";
    std::vector<std::string> pathsToCheck = {adminsPath, membersPath};
    if (includeInvited)
        pathsToCheck.emplace_back(invitedPath);
    for (const auto& path : pathsToCheck) {
        for (const auto& certificate : fileutils::readDirectory(path)) {
            if (path != invitedPath && certificate.find(".crt") == std::string::npos) {
                JAMI_WARN("Incorrect file found: %s/%s", path.c_str(), certificate.c_str());
                continue;
            }
            auto crtUri = certificate;
            if (crtUri.find(".crt") != std::string::npos)
                crtUri = crtUri.substr(0, crtUri.size() - std::string(".crt").size());
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
                          OnDoneCb&& cb)
{
    Json::Value json;
    json["body"] = std::move(message);
    json["type"] = type;
    sendMessage(std::move(json), replyTo, std::move(cb));
}

void
Conversation::sendMessage(Json::Value&& value, const std::string& replyTo, OnDoneCb&& cb)
{
    if (!replyTo.empty()) {
        auto commit = pimpl_->repository_->getCommit(replyTo);
        if (commit == std::nullopt) {
            JAMI_ERR("Replying to invalid commit %s", replyTo.c_str());
            return;
        }
        value["reply-to"] = replyTo;
    }
    dht::ThreadPool::io().run([w = weak(), value = std::move(value), cb = std::move(cb)] {
        if (auto sthis = w.lock()) {
            auto acc = sthis->pimpl_->account_.lock();
            if (!acc)
                return;
            std::unique_lock<std::mutex> lk(sthis->pimpl_->writeMtx_);
            Json::StreamWriterBuilder wbuilder;
            wbuilder["commentStyle"] = "None";
            wbuilder["indentation"] = "";
            auto commit = sthis->pimpl_->repository_->commitMessage(
                Json::writeString(wbuilder, value));
            sthis->pimpl_->sending_.emplace_back(commit);
            sthis->pimpl_->saveSending();
            sthis->clearFetched();
            lk.unlock();
            sthis->pimpl_->announce(commit);
            emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(
                acc->getAccountID(),
                sthis->id(),
                acc->getUsername(),
                commit,
                static_cast<int>(DRing::Account::MessageStates::SENDING));
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
            Json::StreamWriterBuilder wbuilder;
            wbuilder["commentStyle"] = "None";
            wbuilder["indentation"] = "";
            std::unique_lock<std::mutex> lk(sthis->pimpl_->writeMtx_);
            for (const auto& message : messages) {
                auto commit = sthis->pimpl_->repository_->commitMessage(
                    Json::writeString(wbuilder, message));
                commits.emplace_back(std::move(commit));
            }
            lk.unlock();
            sthis->pimpl_->announce(commits);
            sthis->clearFetched();
            if (cb)
                cb(commits);
        }
    });
}

void
Conversation::loadMessages(const OnLoadMessages& cb, const std::string& fromMessage, size_t n)
{
    if (!cb)
        return;
    dht::ThreadPool::io().run([w = weak(), cb = std::move(cb), fromMessage, n] {
        if (auto sthis = w.lock()) {
            cb(sthis->pimpl_->loadMessages(fromMessage, "", n));
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
Conversation::loadMessages(const OnLoadMessages& cb,
                           const std::string& fromMessage,
                           const std::string& toMessage)
{
    if (!cb)
        return;
    dht::ThreadPool::io().run([w = weak(), cb = std::move(cb), fromMessage, toMessage] {
        if (auto sthis = w.lock()) {
            cb(sthis->pimpl_->loadMessages(fromMessage, toMessage, 0));
        }
    });
}

std::string
Conversation::lastCommitId() const
{
    auto messages = pimpl_->loadMessages("", "", 1);
    if (messages.empty())
        return {};
    return messages.front().at(ConversationMapKeys::ID);
}

bool
Conversation::fetchFrom(const std::string& uri)
{
    return pimpl_->repository_->fetch(uri);
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
        JAMI_WARN("Could not get HEAD of %s", uri.c_str());
        return {};
    }

    // Validate commit
    auto [newCommits, err] = repository_->validFetch(uri);
    if (newCommits.empty()) {
        if (err)
            JAMI_ERR("Could not validate history with %s", uri.c_str());
        repository_->removeBranchWith(uri);
        return {};
    }

    // If validated, merge
    auto [ok, cid] = repository_->merge(remoteHead);
    if (!ok) {
        JAMI_ERR("Could not merge history with %s", uri.c_str());
        repository_->removeBranchWith(uri);
        return {};
    }
    if (!cid.empty()) {
        // A merge commit was generated, should be added in new commits
        auto commit = repository_->getCommit(cid);
        if (commit != std::nullopt)
            newCommits.emplace_back(*commit);
    }

    JAMI_DBG("Successfully merge history with %s", uri.c_str());
    auto result = repository_->convCommitToMap(newCommits);
    for (const auto& commit : result) {
        auto it = commit.find("type");
        if (it != commit.end() && it->second == "member") {
            repository_->refreshMembers();
        }
    }
    return result;
}

void
Conversation::pull(const std::string& deviceId, OnPullCb&& cb, std::string commitId)
{
    std::lock_guard<std::mutex> lk(pimpl_->pullcbsMtx_);
    auto isInProgress = not pimpl_->pullcbs_.empty();
    pimpl_->pullcbs_.emplace_back(deviceId, std::move(commitId), std::move(cb));
    if (isInProgress)
        return;
    dht::ThreadPool::io().run([w = weak()] {
        if (auto sthis_ = w.lock())
            sthis_->pimpl_->pull();
    });
}

void
Conversation::Impl::pull()
{
    auto& repo = repository_;

    std::string deviceId, commitId;
    OnPullCb cb;
    while (true) {
        decltype(pullcbs_)::value_type pullcb;
        decltype(fetchingRemotes_.begin()) it;
        {
            std::lock_guard<std::mutex> lk(pullcbsMtx_);
            if (pullcbs_.empty())
                return;
            auto& elem = pullcbs_.front();
            deviceId = std::move(std::get<0>(elem));
            commitId = std::move(std::get<1>(elem));
            cb = std::move(std::get<2>(elem));
            pullcbs_.pop_front();

            // Check if already using this remote, if so, no need to pull yet
            // One pull at a time to avoid any early EOF or fetch errors.
            auto itr = fetchingRemotes_.emplace(deviceId);
            if (!itr.second) {
                // Go to next pull
                pullcbs_.emplace_back(std::move(deviceId), std::move(commitId), std::move(cb));
                continue;
            }
            it = itr.first;
        }
        // If recently fetched, the commit can already be there, so no need to do complex operations
        if (commitId != "" && repo->getCommit(commitId, false) != std::nullopt) {
            cb(true);
            std::lock_guard<std::mutex> lk(pullcbsMtx_);
            fetchingRemotes_.erase(it);
            continue;
        }
        // Pull from remote
        auto fetched = repo->fetch(deviceId);
        {
            std::lock_guard<std::mutex> lk(pullcbsMtx_);
            fetchingRemotes_.erase(it);
        }

        if (!fetched) {
            cb(false);
            continue;
        }
        auto oldHead = repo->getHead();
        std::string newHead = oldHead;
        std::unique_lock<std::mutex> lk(writeMtx_);
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
                auto updatedCommits = loadMessages("", mergeBase);
                // We announce commits from oldest to update to newest. This generally avoid
                // to get detached commits until they are all announced.
                std::reverse(std::begin(updatedCommits), std::end(updatedCommits));
                announce(updatedCommits);
            } else {
                announce(commits);
            }
        }
        lk.unlock();
        if (cb)
            cb(true);
        // Announce if profile changed
        if (oldHead != newHead) {
            auto diffStats = repo->diffStats(newHead, oldHead);
            auto changedFiles = repo->changedFiles(diffStats);
            if (find(changedFiles.begin(), changedFiles.end(), "profile.vcf")
                != changedFiles.end()) {
                if (auto account = account_.lock())
                    emitSignal<DRing::ConversationSignal::ConversationProfileUpdated>(
                        account->getAccountID(), repo->id(), repo->infos());
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
    JAMI_INFO() << "Sync " << id() << " with " << deviceId;
    pull(deviceId, std::move(cb), commitId);
    // For waiting request, downloadFile
    for (const auto& wr : dataTransfer()->waitingRequests())
        downloadFile(wr.interactionId, wr.fileId, wr.path, member, deviceId);
    // VCard sync for member
    if (auto account = pimpl_->account_.lock()) {
        if (not account->needToSendProfile(member, deviceId)) {
            JAMI_INFO() << "Peer " << deviceId << " already got an up-to-date vcard";
            return;
        }
        // We need a new channel
        account->transferFile(id(), account->profilePath(), deviceId, "profile.vcf", "");
    }
}

std::map<std::string, std::string>
Conversation::generateInvitation() const
{
    // Invite the new member to the conversation
    Json::Value root;
    auto& metadata = root[ConversationMapKeys::METADATAS];
    for (const auto& [k, v] : infos()) {
        metadata[k] = v;
    }
    root[ConversationMapKeys::CONVERSATIONID] = id();
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return {{"application/invite+json", Json::writeString(wbuilder, root)}};
}

std::string
Conversation::leave()
{
    setRemovingFlag();
    std::lock_guard<std::mutex> lk(pimpl_->writeMtx_);
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
        fileutils::removeAll(pimpl_->conversationDataPath_, true);
    if (!pimpl_->repository_)
        return;
    std::lock_guard<std::mutex> lk(pimpl_->writeMtx_);
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
            std::unique_lock<std::mutex> lk(sthis->pimpl_->writeMtx_);
            auto commit = repo->updateInfos(map);
            sthis->pimpl_->announce(commit);
            lk.unlock();
            if (cb)
                cb(!commit.empty(), commit);
            if (auto account = sthis->pimpl_->account_.lock())
                emitSignal<DRing::ConversationSignal::ConversationProfileUpdated>(
                    account->getAccountID(), repo->id(), repo->infos());
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
    auto filePath = fmt::format("{}/preferences", pimpl_->conversationDataPath_);
    auto prefs = map;
    auto itLast = prefs.find(LAST_MODIFIED);
    if (itLast != prefs.end()) {
        if (fileutils::isFile(filePath)) {
            auto lastModified = fileutils::lastWriteTime(filePath);
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
    emitSignal<DRing::ConversationSignal::ConversationPreferencesUpdated>(pimpl_->accountId_,
                                                                          id(),
                                                                          std::move(prefs));
}

std::map<std::string, std::string>
Conversation::preferences(bool includeLastModified) const
{
    try {
        std::map<std::string, std::string> preferences;
        auto filePath = fmt::format("{}/preferences", pimpl_->conversationDataPath_);
        auto file = fileutils::loadFile(filePath);
        msgpack::object_handle oh = msgpack::unpack((const char*) file.data(), file.size());
        oh.get().convert(preferences);
        if (includeLastModified)
            preferences[LAST_MODIFIED] = fmt::format("{}", fileutils::lastWriteTime(filePath));
        return preferences;
    } catch (const std::exception& e) {
    }
    return {};
}

std::vector<uint8_t>
Conversation::vCard() const
{
    try {
        return fileutils::loadFile(pimpl_->repoPath() + DIR_SEPARATOR_STR + "profile.vcf");
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
                                   bool verifyShaSum) const
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
        || commit->at("type") != "application/data-transfer+json")
        return false;

    auto path = dataTransfer()->path(fileId);

    if (!fileutils::isFile(path)) {
        // Check if dangling symlink
        if (fileutils::isSymLink(path)) {
            fileutils::remove(path, true);
        }
        JAMI_DBG("[Account %s] %s asked for non existing file %s in %s",
                 pimpl_->accountId_.c_str(),
                 member.c_str(),
                 fileId.c_str(),
                 id().c_str());
        return false;
    }
    // Check that our file is correct before sending
    if (verifyShaSum && commit->at("sha3sum") != fileutils::sha3File(path)) {
        JAMI_DBG("[Account %s] %s asked for file %s in %s, but our version is not complete",
                 pimpl_->accountId_.c_str(),
                 member.c_str(),
                 fileId.c_str(),
                 id().c_str());
        return false;
    }
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
    if (commit == std::nullopt || commit->find("type") == commit->end()
        || commit->find("sha3sum") == commit->end() || commit->find("tid") == commit->end()
        || commit->at("type") != "application/data-transfer+json") {
        JAMI_ERR() << "Cannot download file without linked interaction " << fileId;
        return false;
    }
    auto sha3sum = commit->at("sha3sum");
    auto size_str = commit->at("totalSize");
    auto totalSize = to_int<size_t>(size_str);

    // Be sure to not lock conversation
    dht::ThreadPool().io().run(
        [w = weak(), deviceId, fileId, interactionId, sha3sum, path, totalSize, start, end] {
            if (auto shared = w.lock()) {
                auto acc = shared->pimpl_->account_.lock();
                if (!acc)
                    return;
                shared->dataTransfer()->waitForTransfer(fileId,
                                                        interactionId,
                                                        sha3sum,
                                                        path,
                                                        totalSize);
                acc->askForFileChannel(shared->id(), deviceId, interactionId, fileId, start, end);
            }
        });
    return true;
}

void
Conversation::clearFetched()
{
    std::lock_guard<std::mutex> lk(pimpl_->fetchedDevicesMtx_);
    pimpl_->fetchedDevices_.clear();
    pimpl_->saveFetched();
}

bool
Conversation::needsFetch(const std::string& deviceId) const
{
    std::lock_guard<std::mutex> lk(pimpl_->fetchedDevicesMtx_);
    return pimpl_->fetchedDevices_.find(deviceId) == pimpl_->fetchedDevices_.end();
}

void
Conversation::hasFetched(const std::string& deviceId, const std::string& commitId)
{
    dht::ThreadPool::io().run([w = weak(), deviceId, commitId]() {
        auto sthis = w.lock();
        if (!sthis)
            return;
        {
            std::lock_guard<std::mutex> lk(sthis->pimpl_->fetchedDevicesMtx_);
            sthis->pimpl_->fetchedDevices_.emplace(deviceId);
            sthis->pimpl_->saveFetched();
        }
        // Update sent status
        std::lock_guard<std::mutex> lk(sthis->pimpl_->writeMtx_);
        auto itCommit = std::find(sthis->pimpl_->sending_.begin(),
                                  sthis->pimpl_->sending_.end(),
                                  commitId);
        if (itCommit != sthis->pimpl_->sending_.end()) {
            auto acc = sthis->pimpl_->account_.lock();
            // Clear fetched commits and mark it as announced
            auto end = std::next(itCommit);
            for (auto it = sthis->pimpl_->sending_.begin(); it != end; ++it) {
                emitSignal<DRing::ConfigurationSignal::AccountMessageStatusChanged>(
                    acc->getAccountID(),
                    sthis->id(),
                    acc->getUsername(),
                    *it,
                    static_cast<int>(DRing::Account::MessageStates::SENT));
            }
            sthis->pimpl_->sending_.erase(sthis->pimpl_->sending_.begin(), end);
            sthis->pimpl_->saveSending();
        }
    });
}

bool
Conversation::setMessageDisplayed(const std::string& uri, const std::string& interactionId)
{
    if (auto acc = pimpl_->account_.lock()) {
        {
            std::lock_guard<std::mutex> lk(pimpl_->lastDisplayedMtx_);
            if (pimpl_->lastDisplayed_[uri] == interactionId)
                return false;
            pimpl_->lastDisplayed_[uri] = interactionId;
            pimpl_->saveLastDisplayed();
        }
        if (uri == acc->getUsername() && pimpl_->lastDisplayedUpdatedCb_)
            pimpl_->lastDisplayedUpdatedCb_(pimpl_->repository_->id(), interactionId);
    }
    return true;
}

void
Conversation::updateLastDisplayed(const std::string& lastDisplayed)
{
    auto acc = pimpl_->account_.lock();
    if (!acc or !pimpl_->repository_)
        return;

    // Here, there can be several different scenarios
    // 1. lastDisplayed is the current last displayed interaction. Nothing to do.
    std::unique_lock<std::mutex> lk(pimpl_->lastDisplayedMtx_);
    auto& currentLastDisplayed = pimpl_->lastDisplayed_[acc->getUsername()];
    if (lastDisplayed == currentLastDisplayed)
        return;

    auto updateLastDisplayed = [&]() {
        currentLastDisplayed = lastDisplayed;
        pimpl_->saveLastDisplayed();
        lk.unlock();
        if (pimpl_->lastDisplayedUpdatedCb_)
            pimpl_->lastDisplayedUpdatedCb_(id(), lastDisplayed);
    };

    auto hasCommit = pimpl_->repository_->getCommit(lastDisplayed, false) != std::nullopt;

    // 2. lastDisplayed can be a future commit, not fetched yet
    // In this case, we can cache it here, and check future announces to update it
    if (!hasCommit) {
        pimpl_->lastDisplayed_[ConversationMapKeys::CACHED] = lastDisplayed;
        pimpl_->saveLastDisplayed();
        return;
    }

    // 3. There is no lastDisplayed yet
    if (currentLastDisplayed.empty()) {
        updateLastDisplayed();
        return;
    }

    // 4. lastDisplayed is present in the repository. In this can, if it's a more recent
    // commit than the current one, update it, else drop it.
    auto commitsSinceLast = pimpl_->repository_->log("", lastDisplayed, false, true).size();
    auto commitsSincePreviousLast = pimpl_->repository_->log("", currentLastDisplayed, false, true)
                                        .size();
    if (commitsSincePreviousLast > commitsSinceLast)
        updateLastDisplayed();
}

void
Conversation::onLastDisplayedUpdated(
    std::function<void(const std::string&, const std::string&)>&& lastDisplayedUpdatedCb)
{
    pimpl_->lastDisplayedUpdatedCb_ = std::move(lastDisplayedUpdatedCb);
}

uint32_t
Conversation::countInteractions(const std::string& toId,
                                const std::string& fromId,
                                const std::string& authorUri) const
{
    // Log but without content to avoid costly convertions.
    return pimpl_->repository_->log(fromId, toId, false, true, authorUri).size();
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
            auto acc = sthis->pimpl_->account_.lock();
            if (!acc)
                return;
            auto commits = sthis->pimpl_->repository_->search(filter);
            if (commits.size() > 0)
                emitSignal<DRing::ConversationSignal::MessagesFound>(req,
                                                                     acc->getAccountID(),
                                                                     sthis->id(),
                                                                     std::move(commits));
            // If we're the latest thread, inform client that the search is finished
            if ((*flag)-- == 1 /* decrement return the old value */) {
                emitSignal<DRing::ConversationSignal::MessagesFound>(
                    req,
                    acc->getAccountID(),
                    std::string {},
                    std::vector<std::map<std::string, std::string>> {});
            }
        }
    });
}

} // namespace jami
