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
#include "conversation.h"

#include "fileutils.h"
#include "jamiaccount.h"
#include "conversationrepository.h"
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

ConvInfo::ConvInfo(const Json::Value& json)
{
    id = json["id"].asString();
    created = json["created"].asLargestUInt();
    removed = json["removed"].asLargestUInt();
    erased = json["erased"].asLargestUInt();
    for (const auto& v : json["members"]) {
        members.emplace_back(v["uri"].asString());
    }
}

Json::Value
ConvInfo::toJson() const
{
    Json::Value json;
    json["id"] = id;
    json["created"] = Json::Int64(created);
    if (removed) {
        json["removed"] = Json::Int64(removed);
    }
    if (erased) {
        json["erased"] = Json::Int64(erased);
    }
    for (const auto& m : members) {
        Json::Value member;
        member["uri"] = m;
        json["members"].append(member);
    }
    return json;
}

// ConversationRequest
ConversationRequest::ConversationRequest(const Json::Value& json)
{
    received = json["received"].asLargestUInt();
    declined = json["declined"].asLargestUInt();
    from = json["from"].asString();
    conversationId = json["conversationId"].asString();
    auto& md = json["metadatas"];
    for (const auto& member : md.getMemberNames()) {
        metadatas.emplace(member, md[member].asString());
    }
}

Json::Value
ConversationRequest::toJson() const
{
    Json::Value json;
    json["conversationId"] = conversationId;
    json["from"] = from;
    json["received"] = static_cast<uint32_t>(received);
    if (declined)
        json["declined"] = static_cast<uint32_t>(declined);
    for (const auto& [key, value] : metadatas) {
        json["metadatas"][key] = value;
    }
    return json;
}

std::map<std::string, std::string>
ConversationRequest::toMap() const
{
    auto result = metadatas;
    result["id"] = conversationId;
    result["from"] = from;
    if (declined)
        result["declined"] = std::to_string(declined);
    result["received"] = std::to_string(received);
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
            transferManager_ = std::make_shared<TransferManager>(shared->getAccountID(),
                                                                 repository_->id());
            conversationDataPath_ = fileutils::get_data_dir() + DIR_SEPARATOR_STR
                                    + shared->getAccountID() + DIR_SEPARATOR_STR
                                    + "conversation_data" + DIR_SEPARATOR_STR + repository_->id();
            fetchedPath_ = conversationDataPath_ + DIR_SEPARATOR_STR + "fetched";
            lastDisplayedPath_ = conversationDataPath_ + DIR_SEPARATOR_STR + "lastDisplayed";
            loadFetched();
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
        announce(convCommitToMap(convcommits));
    }

    void announce(const std::vector<std::map<std::string, std::string>>& commits) const
    {
        auto shared = account_.lock();
        if (!shared or !repository_) {
            return;
        }
        auto convId = repository_->id();
        auto ok = !commits.empty();
        auto lastId = ok ? commits.rbegin()->at("id") : "";
        if (ok) {
            bool announceMember = false;
            for (const auto& c : commits) {
                // Announce member events
                if (c.at("type") == "member") {
                    if (c.find("uri") != c.end() && c.find("action") != c.end()) {
                        auto uri = c.at("uri");
                        auto actionStr = c.at("action");
                        auto action = -1;
                        if (actionStr == "add")
                            action = 0;
                        else if (actionStr == "join")
                            action = 1;
                        else if (actionStr == "remove")
                            action = 2;
                        else if (actionStr == "ban")
                            action = 3;
                        if (action != -1) {
                            announceMember = true;
                            emitSignal<DRing::ConversationSignal::ConversationMemberEvent>(
                                shared->getAccountID(), convId, uri, action);
                        }
                    }
                }
#ifdef ENABLE_PLUGIN
                auto& pluginChatManager
                    = Manager::instance().getJamiPluginManager().getChatServicesManager();
                if (pluginChatManager.hasHandlers()) {
                    auto cm = std::make_shared<JamiMessage>(shared->getAccountID(),
                                                            convId,
                                                            c.at("author") != shared->getUsername(),
                                                            c,
                                                            false);
                    cm->isSwarm = true;
                    pluginChatManager.publishMessage(std::move(cm));
                }
#endif
                // announce message
                emitSignal<DRing::ConversationSignal::MessageReceived>(shared->getAccountID(),
                                                                       convId,
                                                                       c);
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

    void loadLastDisplayed()
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

    void saveLastDisplayed()
    {
        std::ofstream file(lastDisplayedPath_, std::ios::trunc | std::ios::binary);
        msgpack::pack(file, lastDisplayed_);
    }

    std::unique_ptr<ConversationRepository> repository_;
    std::weak_ptr<JamiAccount> account_;
    std::atomic_bool isRemoving_ {false};
    std::vector<std::map<std::string, std::string>> convCommitToMap(
        const std::vector<ConversationCommit>& commits) const;
    std::optional<std::map<std::string, std::string>> convCommitToMap(
        const ConversationCommit& commit) const;
    std::vector<std::map<std::string, std::string>> loadMessages(const std::string& fromMessage = "",
                                                                 const std::string& toMessage = "",
                                                                 size_t n = 0);

    std::mutex pullcbsMtx_ {};
    std::set<std::string> fetchingRemotes_ {}; // store current remote in fetch
    std::deque<std::tuple<std::string, std::string, OnPullCb>> pullcbs_ {};
    std::shared_ptr<TransferManager> transferManager_ {};
    std::string conversationDataPath_ {};
    std::string fetchedPath_ {};
    std::mutex fetchedDevicesMtx_ {};
    std::set<std::string> fetchedDevices_ {};
    std::string lastDisplayedPath_ {};
    std::mutex lastDisplayedMtx_ {};
    std::map<std::string, std::string> lastDisplayed_ {};
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
    auto shared = account_.lock();
    if (!shared)
        return {};
    return fileutils::get_data_dir() + DIR_SEPARATOR_STR + shared->getAccountID()
           + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR + repository_->id();
}

std::optional<std::map<std::string, std::string>>
Conversation::Impl::convCommitToMap(const ConversationCommit& commit) const
{
    auto authorDevice = commit.author.email;
    auto cert = tls::CertificateStore::instance().getCertificate(authorDevice);
    if (!cert || !cert->issuer) {
        JAMI_WARN("No author found for commit %s, reload certificates", commit.id.c_str());
        if (repository_)
            repository_->pinCertificates();
        // Get certificate from repo
        try {
            cert = tls::CertificateStore::instance().getCertificate(authorDevice);
            if (!cert || !cert->issuer) {
                JAMI_ERR("No author found for commit %s", commit.id.c_str());
                return std::nullopt;
            }
        } catch (...) {
            return std::nullopt;
        }
    }
    auto authorId = cert->issuer->getId().toString();
    std::string parents;
    auto parentsSize = commit.parents.size();
    for (std::size_t i = 0; i < parentsSize; ++i) {
        parents += commit.parents[i];
        if (i != parentsSize - 1)
            parents += ",";
    }
    std::string type {};
    if (parentsSize > 1) {
        type = "merge";
    }
    std::string body {};
    std::map<std::string, std::string> message;
    if (type.empty()) {
        std::string err;
        Json::Value cm;
        Json::CharReaderBuilder rbuilder;
        auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
        if (reader->parse(commit.commit_msg.data(),
                          commit.commit_msg.data() + commit.commit_msg.size(),
                          &cm,
                          &err)) {
            for (auto const& id : cm.getMemberNames()) {
                if (id == "type") {
                    type = cm[id].asString();
                    continue;
                }
                message.emplace(id, cm[id].asString());
            }
        } else {
            JAMI_WARN("%s", err.c_str());
        }
    }
    if (type == "application/data-transfer+json") {
        // Avoid the client to do the concatenation
        message["fileId"] = commit.id + "_" + message["tid"];
        auto extension = fileutils::getFileExtension(message["displayName"]);
        if (!extension.empty())
            message["fileId"] += "." + extension;
    }
    message["id"] = commit.id;
    message["parents"] = parents;
    message["linearizedParent"] = commit.linearized_parent;
    message["author"] = authorId;
    message["type"] = type;
    message["timestamp"] = std::to_string(commit.timestamp);

    return message;
}

std::vector<std::map<std::string, std::string>>
Conversation::Impl::convCommitToMap(const std::vector<ConversationCommit>& commits) const
{
    std::vector<std::map<std::string, std::string>> result = {};
    result.reserve(commits.size());
    for (const auto& commit : commits) {
        auto message = convCommitToMap(commit);
        if (message == std::nullopt)
            continue;
        result.emplace_back(*message);
    }
    return result;
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
    return convCommitToMap(convCommits);
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
        JAMI_WARN("Could not add member %s because this member is banned", contactUri.c_str());
        cb(false, "");
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
            // Vote for removal
            std::unique_lock<std::mutex> lk(sthis->pimpl_->writeMtx_);
            auto voteCommit = sthis->pimpl_->repository_->voteKick(contactUri, isDevice);
            if (voteCommit.empty()) {
                JAMI_WARN("Kicking %s failed", contactUri.c_str());
                cb(false, "");
                return;
            }

            auto lastId = voteCommit;
            std::vector<std::string> commits;
            commits.emplace_back(voteCommit);

            // If admin, check vote
            auto resolveCommit = sthis->pimpl_->repository_->resolveVote(contactUri, isDevice);
            if (!resolveCommit.empty()) {
                commits.emplace_back(resolveCommit);
                lastId = resolveCommit;
                JAMI_WARN("Vote solved for %s. %s banned",
                          contactUri.c_str(),
                          isDevice ? "Device" : "Member");
            }
            sthis->pimpl_->announce(commits);
            lk.unlock();
            if (cb)
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
        mm["lastDisplayed"] = std::move(lastDisplayed);
        result.emplace_back(std::move(mm));
    }
    return result;
}

std::vector<std::string>
Conversation::memberUris(std::string_view filter) const
{
    return pimpl_->repository_->memberUris(filter);
}

std::string
Conversation::join()
{
    auto shared = pimpl_->account_.lock();
    if (!shared)
        return {};
    return pimpl_->repository_->join();
}

bool
Conversation::isMember(const std::string& uri, bool includeInvited) const
{
    auto shared = pimpl_->account_.lock();
    if (!shared)
        return false;

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
Conversation::isBanned(const std::string& uri, bool isDevice) const
{
    auto shared = pimpl_->account_.lock();
    if (!shared)
        return true;

    auto type = isDevice ? "devices" : "members";
    auto bannedPath = pimpl_->repoPath() + DIR_SEPARATOR_STR + "banned" + DIR_SEPARATOR_STR + type
                      + DIR_SEPARATOR_STR + uri + ".crt";
    return fileutils::isFile(bannedPath);
}

void
Conversation::sendMessage(std::string&& message,
                          const std::string& type,
                          const std::string& parent,
                          OnDoneCb&& cb)
{
    Json::Value json;
    json["body"] = std::move(message);
    json["type"] = type;
    sendMessage(std::move(json), parent, std::move(cb));
}

void
Conversation::sendMessage(Json::Value&& value, const std::string& /*parent*/, OnDoneCb&& cb)
{
    dht::ThreadPool::io().run([w = weak(), value = std::move(value), cb = std::move(cb)] {
        if (auto sthis = w.lock()) {
            auto shared = sthis->pimpl_->account_.lock();
            if (!shared)
                return;
            std::unique_lock<std::mutex> lk(sthis->pimpl_->writeMtx_);
            Json::StreamWriterBuilder wbuilder;
            wbuilder["commentStyle"] = "None";
            wbuilder["indentation"] = "";
            auto commit = sthis->pimpl_->repository_->commitMessage(
                Json::writeString(wbuilder, value));
            sthis->clearFetched();
            lk.unlock();
            if (cb)
                cb(!commit.empty(), commit);
            sthis->pimpl_->announce(commit);
        }
    });
}

void
Conversation::sendMessages(std::vector<Json::Value>&& messages,
                           const std::string& /*parent*/,
                           OnMultiDoneCb&& cb)
{
    dht::ThreadPool::io().run([w = weak(), messages = std::move(messages), cb = std::move(cb)] {
        if (auto sthis = w.lock()) {
            auto shared = sthis->pimpl_->account_.lock();
            if (!shared)
                return;
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
    return pimpl_->convCommitToMap(*commit);
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
    return messages.front().at("id");
}

bool
Conversation::fetchFrom(const std::string& uri)
{
    return pimpl_->repository_->fetch(uri);
}

std::vector<std::map<std::string, std::string>>
Conversation::mergeHistory(const std::string& uri)
{
    if (not pimpl_ or not pimpl_->repository_) {
        JAMI_WARN("Invalid repo. Abort merge");
        return {};
    }
    auto remoteHead = pimpl_->repository_->remoteHead(uri);
    if (remoteHead.empty()) {
        JAMI_WARN("Could not get HEAD of %s", uri.c_str());
        return {};
    }

    // Validate commit
    auto [newCommits, err] = pimpl_->repository_->validFetch(uri);
    if (newCommits.empty()) {
        if (err)
            JAMI_ERR("Could not validate history with %s", uri.c_str());
        pimpl_->repository_->removeBranchWith(uri);
        return {};
    }

    // If validated, merge
    auto [ok, cid] = pimpl_->repository_->merge(remoteHead);
    if (!ok) {
        JAMI_ERR("Could not merge history with %s", uri.c_str());
        pimpl_->repository_->removeBranchWith(uri);
        return {};
    }
    if (!cid.empty()) {
        // A merge commit was generated, should be added in new commits
        auto commit = pimpl_->repository_->getCommit(cid);
        if (commit != std::nullopt)
            newCommits.emplace_back(*commit);
    }

    JAMI_DBG("Successfully merge history with %s", uri.c_str());
    auto result = pimpl_->convCommitToMap(newCommits);
    for (const auto& commit : result) {
        auto it = commit.find("type");
        if (it != commit.end() && it->second == "member") {
            pimpl_->repository_->refreshMembers();
        }
    }
    return result;
}

void
Conversation::pull(const std::string& deviceId, OnPullCb&& cb, std::string commitId)
{
    std::lock_guard<std::mutex> lk(pimpl_->pullcbsMtx_);
    auto isInProgress = not pimpl_->pullcbs_.empty();
    pimpl_->pullcbs_.emplace_back(
        std::make_tuple<std::string, std::string, OnPullCb>(std::string(deviceId),
                                                            std::move(commitId),
                                                            std::move(cb)));
    if (isInProgress)
        return;
    dht::ThreadPool::io().run([w = weak()] {
        auto sthis_ = w.lock();
        if (!sthis_)
            return;

        std::string deviceId, commitId;
        OnPullCb cb;
        while (true) {
            decltype(sthis_->pimpl_->pullcbs_)::value_type pullcb;
            decltype(sthis_->pimpl_->fetchingRemotes_.begin()) it;
            {
                std::lock_guard<std::mutex> lk(sthis_->pimpl_->pullcbsMtx_);
                if (sthis_->pimpl_->pullcbs_.empty())
                    return;
                auto elem = sthis_->pimpl_->pullcbs_.front();
                deviceId = std::get<0>(elem);
                commitId = std::get<1>(elem);
                cb = std::move(std::get<2>(elem));
                sthis_->pimpl_->pullcbs_.pop_front();

                // Check if already using this remote, if so, no need to pull yet
                // One pull at a time to avoid any early EOF or fetch errors.
                if (sthis_->pimpl_->fetchingRemotes_.find(deviceId)
                    != sthis_->pimpl_->fetchingRemotes_.end()) {
                    sthis_->pimpl_->pullcbs_.emplace_back(
                        std::make_tuple<std::string, std::string, OnPullCb>(std::string(deviceId),
                                                                            std::move(commitId),
                                                                            std::move(cb)));
                    // Go to next pull
                    continue;
                }
                auto itr = sthis_->pimpl_->fetchingRemotes_.emplace(deviceId);
                if (!itr.second) {
                    cb(false);
                    continue;
                }
                it = itr.first;
            }
            // If recently fetched, the commit can already be there, so no need to do complex operations
            if (commitId != ""
                && sthis_->pimpl_->repository_->getCommit(commitId, false) != std::nullopt) {
                cb(true);
                std::lock_guard<std::mutex> lk(sthis_->pimpl_->pullcbsMtx_);
                sthis_->pimpl_->fetchingRemotes_.erase(it);
                continue;
            }
            // Pull from remote
            auto fetched = sthis_->fetchFrom(deviceId);
            {
                std::lock_guard<std::mutex> lk(sthis_->pimpl_->pullcbsMtx_);
                sthis_->pimpl_->fetchingRemotes_.erase(it);
            }

            if (!fetched) {
                cb(false);
                continue;
            }
            std::unique_lock<std::mutex> lk(sthis_->pimpl_->writeMtx_);
            auto newCommits = sthis_->mergeHistory(deviceId);
            sthis_->pimpl_->announce(newCommits);
            lk.unlock();
            if (cb)
                cb(true);
        }
    });
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
        if (not account->needToSendProfile(deviceId)) {
            JAMI_INFO() << "Peer " << deviceId << " already got an up-to-date vcard";
            return;
        }
        // We need a new channel
        account->transferFile(id(), std::string(account->profilePath()), deviceId, "profile.vcf", "");
    }
}

std::map<std::string, std::string>
Conversation::generateInvitation() const
{
    // Invite the new member to the conversation
    std::map<std::string, std::string> invite;
    Json::Value root;
    for (const auto& [k, v] : infos()) {
        root["metadatas"][k] = v;
    }
    root["conversationId"] = id();
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    invite["application/invite+json"] = Json::writeString(wbuilder, root);
    return invite;
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
            std::unique_lock<std::mutex> lk(sthis->pimpl_->writeMtx_);
            auto commit = sthis->pimpl_->repository_->updateInfos(map);
            sthis->pimpl_->announce(commit);
            lk.unlock();
            if (cb)
                cb(!commit.empty(), commit);
        }
    });
}

std::map<std::string, std::string>
Conversation::infos() const
{
    return pimpl_->repository_->infos();
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
                                   std::string_view fileId,
                                   bool verifyShaSum) const
{
    if (!isMember(member))
        return false;

    auto account = pimpl_->account_.lock();
    if (!account)
        return false;

    auto sep = fileId.find('_');
    if (sep == std::string::npos)
        return false;

    auto interactionId = fileId.substr(0, sep);
    auto commit = getCommit(std::string(interactionId));
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
        JAMI_DBG("[Account %s] %s asked for non existing file %.*s in %s",
                 account->getAccountID().c_str(),
                 member.c_str(),
                 (int)fileId.size(), fileId.data(),
                 id().c_str());
        return false;
    }
    // Check that our file is correct before sending
    if (verifyShaSum && commit->at("sha3sum") != fileutils::sha3File(path)) {
        JAMI_DBG("[Account %s] %s asked for file %.*s in %s, but our version is not complete",
                 account->getAccountID().c_str(),
                 member.c_str(),
                 (int)fileId.size(), fileId.data(),
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
    std::size_t totalSize;
    std::from_chars(size_str.data(), size_str.data() + size_str.size(), totalSize);

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
Conversation::hasFetched(const std::string& deviceId)
{
    std::lock_guard<std::mutex> lk(pimpl_->fetchedDevicesMtx_);
    pimpl_->fetchedDevices_.emplace(deviceId);
    pimpl_->saveFetched();
}

void
Conversation::setMessageDisplayed(const std::string& uri, const std::string& interactionId)
{
    std::lock_guard<std::mutex> lk(pimpl_->lastDisplayedMtx_);
    pimpl_->lastDisplayed_[uri] = interactionId;
    pimpl_->saveLastDisplayed();
}

uint32_t
Conversation::countInteractions(const std::string& toId,
                                const std::string& fromId,
                                const std::string& authorUri) const
{
    // Log but without content to avoid costly convertions.
    return pimpl_->repository_->log(fromId, toId, false, true, authorUri).size();
}

} // namespace jami