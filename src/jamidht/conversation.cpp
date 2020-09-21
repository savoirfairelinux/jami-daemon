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

#include <json/json.h>

namespace jami {

class Conversation::Impl
{
public:
    Impl(const std::weak_ptr<JamiAccount>& account, const std::string& conversationId)
        : account_(account)
    {
        if (conversationId.empty())
            repository_ = ConversationRepository::createConversation(account);
        else
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
            throw std::logic_error("Couldn't clone repository");
        }
    }
    ~Impl() = default;

    std::string repoPath() const;

    std::unique_ptr<ConversationRepository> repository_;
    std::weak_ptr<JamiAccount> account_;
    std::vector<std::map<std::string, std::string>> loadMessages(const std::string& fromMessage = "",
                                                                 const std::string& toMessage = "",
                                                                 size_t n = 0);
};

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
    std::vector<std::map<std::string, std::string>> result = {};
    for (const auto& commit : convCommits) {
        auto authorDevice = commit.author.email;
        auto cert = tls::CertificateStore::instance().getCertificate(authorDevice);
        if (!cert && cert->issuer) {
            JAMI_WARN("No author found for commit %s", commit.id.c_str());
        }
        auto authorId = cert->issuer->getId().toString();
        std::string parents;
        auto parentsSize = commit.parents.size();
        for (auto i = 0; i < parentsSize; ++i) {
            parents += commit.parents[i];
            if (i != parentsSize - 1)
                parents += ",";
        }
        std::string type {};
        if (parentsSize > 1) {
            type = "merge";
        }
        // TODO check diff for member
        std::string body {};
        if (type.empty()) {
            std::string err;
            Json::Value cm;
            Json::CharReaderBuilder rbuilder;
            auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
            if (reader->parse(commit.commit_msg.data(),
                              commit.commit_msg.data() + commit.commit_msg.size(),
                              &cm,
                              &err)) {
                type = cm["type"].asString();
                body = cm["body"].asString();
            } else {
                JAMI_WARN("%s", err.c_str());
            }
        }
        std::map<std::string, std::string> message {{"id", commit.id},
                                                    {"parents", parents},
                                                    {"author", authorId},
                                                    {"type", type},
                                                    {"body", body},
                                                    {"timestamp", std::to_string(commit.timestamp)}};
        result.emplace_back(message);
    }
    return result;
}

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
    // Add member files and commit
    return pimpl_->repository_->addMember(contactUri);
}

bool
Conversation::removeMember(const std::string& contactUri)
{
    // TODO
    return true;
}

std::vector<std::map<std::string, std::string>>
Conversation::getMembers(bool includeInvited) const
{
    std::vector<std::map<std::string, std::string>> result;
    auto shared = pimpl_->account_.lock();
    if (!shared)
        return result;

    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + shared->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR
                    + pimpl_->repository_->id();
    auto adminsPath = repoPath + DIR_SEPARATOR_STR + "admins";
    auto membersPath = repoPath + DIR_SEPARATOR_STR + "members";
    auto invitedPath = pimpl_->repoPath() + DIR_SEPARATOR_STR + "invited";
    for (const auto& certificate : fileutils::readDirectory(adminsPath)) {
        if (certificate.find(".crt") == std::string::npos) {
            JAMI_WARN("Incorrect file found: %s/%s", adminsPath.c_str(), certificate.c_str());
            continue;
        }
        std::map<std::string, std::string>
            details {{"uri", certificate.substr(0, certificate.size() - std::string(".crt").size())},
                     {"role", "admin"}};
        result.emplace_back(details);
    }
    for (const auto& certificate : fileutils::readDirectory(membersPath)) {
        if (certificate.find(".crt") == std::string::npos) {
            JAMI_WARN("Incorrect file found: %s/%s", membersPath.c_str(), certificate.c_str());
            continue;
        }
        std::map<std::string, std::string>
            details {{"uri", certificate.substr(0, certificate.size() - std::string(".crt").size())},
                     {"role", "member"}};
        result.emplace_back(details);
    }
    if (includeInvited) {
        for (const auto& uri : fileutils::readDirectory(invitedPath)) {
            std::map<std::string, std::string>
                details {{"uri", uri },
                        {"role", "invited"}};
            result.emplace_back(details);
        }
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
Conversation::isMember(const std::string& uri, bool includeInvited)
{
    auto shared = pimpl_->account_.lock();
    if (!shared)
        return false;

    auto repoPath = fileutils::get_data_dir() + DIR_SEPARATOR_STR + shared->getAccountID()
                    + DIR_SEPARATOR_STR + "conversations" + DIR_SEPARATOR_STR
                    + pimpl_->repository_->id();
    auto invitedPath = repoPath + DIR_SEPARATOR_STR + "invited";
    auto adminsPath = repoPath + DIR_SEPARATOR_STR + "admins";
    auto membersPath = repoPath + DIR_SEPARATOR_STR + "members";
    std::vector<std::string> pathsToCheck = {adminsPath, membersPath};
    if (includeInvited)
        pathsToCheck.emplace_back(invitedPath);
    for (const auto& path : pathsToCheck) {
        for (const auto& certificate : fileutils::readDirectory(path)) {
            if (certificate.find(".crt") == std::string::npos) {
                JAMI_WARN("Incorrect file found: %s/%s", path.c_str(), certificate.c_str());
                continue;
            }
            auto crtUri = certificate.substr(0, certificate.size() - std::string(".crt").size());
            if (crtUri == uri)
                return true;
        }
    }

    return false;
}

std::string
Conversation::sendMessage(const std::string& message,
                          const std::string& type,
                          const std::string& parent)
{
    Json::Value json;
    json["body"] = message;
    json["type"] = type;
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return pimpl_->repository_->commitMessage(Json::writeString(wbuilder, json));
}

std::vector<std::map<std::string, std::string>>
Conversation::loadMessages(const std::string& fromMessage, size_t n)
{
    return pimpl_->loadMessages(fromMessage, "", n);
}

std::vector<std::map<std::string, std::string>>
Conversation::loadMessages(const std::string& fromMessage, const std::string& toMessage)
{
    return pimpl_->loadMessages(fromMessage, toMessage, 0);
}

bool
Conversation::fetchFrom(const std::string& uri)
{
    // TODO check if device id or account id
    return pimpl_->repository_->fetch(uri);
}

bool
Conversation::mergeHistory(const std::string& uri)
{
    if (not pimpl_ or not pimpl_->repository_) {
        JAMI_WARN("Invalid repo. Abort merge");
        return false;
    }
    auto remoteHead = pimpl_->repository_->remoteHead(uri);
    if (remoteHead.empty()) {
        JAMI_WARN("Could not get HEAD of %s", uri.c_str());
        return false;
    }

    // In the future, the diff should be analyzed to know if the
    // history presented by the peer is correct or not.

    if (!pimpl_->repository_->merge(remoteHead)) {
        JAMI_ERR("Could not merge history with %s", uri.c_str());
        return false;
    }
    JAMI_DBG("Successfully merge history with %s", uri.c_str());
    return true;
}

std::map<std::string, std::string>
Conversation::generateInvitation() const
{
    // Invite the new member to the conversation
    std::map<std::string, std::string> invite;
    Json::Value root;
    root["conversationId"] = id();
    // TODO remove, cause the peer cannot trust?
    // Or add signatures?
    for (const auto& member : getMembers()) {
        Json::Value jsonMember;
        for (const auto& [key, value] : member) {
            jsonMember[key] = value;
        }
        root["members"].append(jsonMember);
    }
    // TODO metadatas
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    invite["application/invite+json"] = Json::writeString(wbuilder, root);
    return invite;
}

} // namespace jami