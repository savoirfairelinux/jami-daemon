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

#include <json/json.h>
#include <string_view>
#include <opendht/thread_pool.h>
#include <tuple>

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
    }

    Impl(const std::weak_ptr<JamiAccount>& account, const std::string& conversationId)
        : account_(account)
    {
        repository_ = std::make_unique<ConversationRepository>(account, conversationId);
        if (!repository_) {
            throw std::logic_error("Couldn't create repository");
        }
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
    }
    ~Impl() = default;

    bool isAdmin() const;
    std::string repoPath() const;

    std::unique_ptr<ConversationRepository> repository_;
    std::weak_ptr<JamiAccount> account_;
    std::atomic_bool isRemoving_ {false};
    std::vector<std::map<std::string, std::string>> convCommitToMap(
        const std::vector<ConversationCommit>& commits) const;
    std::vector<std::map<std::string, std::string>> loadMessages(const std::string& fromMessage = "",
                                                                 const std::string& toMessage = "",
                                                                 size_t n = 0);

    std::mutex pullcbsMtx_ {};
    std::set<std::string> fetchingRemotes_ {}; // store current remote in fetch
    std::deque<std::tuple<std::string, std::string, OnPullCb>> pullcbs_ {};

    // Mutex used to protect write index (one commit at a time)
    std::mutex writeMtx_ {};
};

bool
Conversation::Impl::isAdmin() const
{
    auto shared = account_.lock();
    if (!shared)
        return false;

    auto adminsPath = repoPath() + DIR_SEPARATOR_STR + "admins";
    auto cert = shared->identity().second;
    auto uri = cert->getIssuerUID();
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

std::vector<std::map<std::string, std::string>>
Conversation::Impl::convCommitToMap(const std::vector<ConversationCommit>& commits) const
{
    std::vector<std::map<std::string, std::string>> result = {};
    for (const auto& commit : commits) {
        auto authorDevice = commit.author.email;
        auto cert = tls::CertificateStore::instance().getCertificate(authorDevice);
        if (!cert) {
            JAMI_WARN("No author found for commit %s, reload certificates", commit.id.c_str());
            if (repository_)
                repository_->pinCertificates();
            // Get certificate from repo
            try {
                auto certPath = fileutils::getFullPath(repoPath(),
                                                       std::string("devices") + DIR_SEPARATOR_STR
                                                           + authorDevice + ".crt");
                auto deviceCert = fileutils::loadTextFile(certPath);
                cert = std::make_shared<crypto::Certificate>(deviceCert);
                if (!cert) {
                    JAMI_ERR("No author found for commit %s", commit.id.c_str());
                    continue;
                }
            } catch (...) {
                continue;
            }
        }
        auto authorId = cert->getIssuerUID();
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
                    message.insert({id, cm[id].asString()});
                }
            } else {
                JAMI_WARN("%s", err.c_str());
            }
        }
        auto linearizedParent = repository_->linearizedParent(commit.id);
        message["id"] = commit.id;
        message["parents"] = parents;
        message["linearizedParent"] = linearizedParent ? *linearizedParent : "";
        message["author"] = authorId;
        message["type"] = type;
        message["timestamp"] = std::to_string(commit.timestamp);
        result.emplace_back(message);
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

std::string
Conversation::addMember(const std::string& contactUri)
{
    try {
        if (mode() == ConversationMode::ONE_TO_ONE) {
            // Only authorize to add left members
            auto initialMembers = getInitialMembers();
            auto it = std::find(initialMembers.begin(), initialMembers.end(), contactUri);
            if (it == initialMembers.end()) {
                JAMI_WARN("Cannot add new member in one to one conversation");
                return {};
            }
        }
    } catch (const std::exception& e) {
        JAMI_WARN("Cannot get mode: %s", e.what());
        return {};
    }
    if (isMember(contactUri, true)) {
        JAMI_WARN("Could not add member %s because it's already a member", contactUri.c_str());
        return {};
    }
    if (isBanned(contactUri)) {
        JAMI_WARN("Could not add member %s because this member is banned", contactUri.c_str());
        return {};
    }
    // Add member files and commit
    return pimpl_->repository_->addMember(contactUri);
}

bool
Conversation::removeMember(const std::string& contactUri, bool isDevice)
{
    // Check if admin
    if (!pimpl_->isAdmin()) {
        JAMI_WARN("You're not an admin of this repo. Cannot ban %s", contactUri.c_str());
        return false;
    }
    // Vote for removal
    if (pimpl_->repository_->voteKick(contactUri, isDevice).empty()) {
        JAMI_WARN("Kicking %s failed", contactUri.c_str());
        return false;
    }
    // If admin, check vote
    if (!pimpl_->repository_->resolveVote(contactUri, isDevice).empty()) {
        JAMI_WARN("Vote solved for %s. %s banned",
                  contactUri.c_str(),
                  isDevice ? "Device" : "Member");
    }
    return true;
}

std::vector<std::map<std::string, std::string>>
Conversation::getMembers(bool includeInvited) const
{
    std::vector<std::map<std::string, std::string>> result;

    auto shared = pimpl_->account_.lock();
    if (!shared)
        return result;
    auto members = pimpl_->repository_->members();
    for (const auto& member : members) {
        if (member.role == MemberRole::BANNED)
            continue;
        if (member.role == MemberRole::INVITED && !includeInvited)
            continue;
        result.emplace_back(member.map());
    }
    return result;
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

std::string
Conversation::sendMessage(const std::string& message,
                          const std::string& type,
                          const std::string& parent)
{
    Json::Value json;
    json["body"] = message;
    json["type"] = type;
    return sendMessage(json, parent);
}

std::string
Conversation::sendMessage(const Json::Value& value, const std::string& /*parent*/)
{
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    std::lock_guard<std::mutex> lk(pimpl_->writeMtx_);
    return pimpl_->repository_->commitMessage(Json::writeString(wbuilder, value));
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

    std::unique_lock<std::mutex> lk(pimpl_->writeMtx_);
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
    lk.unlock();

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
Conversation::pull(const std::string& uri, OnPullCb&& cb, std::string commitId)
{
    std::lock_guard<std::mutex> lk(pimpl_->pullcbsMtx_);
    auto isInProgress = not pimpl_->pullcbs_.empty();
    pimpl_->pullcbs_.emplace_back(
        std::make_tuple<std::string, std::string, OnPullCb>(std::string(uri),
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
                    cb(false, {});
                    continue;
                }
                it = itr.first;
            }
            // If recently fetched, the commit can already be there, so no need to do complex operations
            if (commitId != ""
                && sthis_->pimpl_->repository_->getCommit(commitId, false) != std::nullopt) {
                cb(true, {});
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
                cb(false, {});
                continue;
            }
            auto newCommits = sthis_->mergeHistory(deviceId);
            auto ok = !newCommits.empty();
            if (cb)
                cb(ok, std::move(newCommits));
        }
    });
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

std::string
Conversation::updateInfos(const std::map<std::string, std::string>& map)
{
    return pimpl_->repository_->updateInfos(map);
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

} // namespace jami