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

namespace jami
{

Conversation::Conversation(const std::weak_ptr<JamiAccount>& account, const std::string& conversationId)
: account_(account)
{
    if (conversationId.empty())
        repository_ = ConversationRepository::createConversation(account);
    else
        repository_ = std::make_unique<ConversationRepository>(account, conversationId);
}

Conversation::Conversation(const std::weak_ptr<JamiAccount>& account, const std::string& remoteDevice, const std::string& conversationId)
: account_(account)
{
    repository_ = ConversationRepository::cloneConversation(account, remoteDevice, conversationId);
}


Conversation::~Conversation()
{

}

std::string
Conversation::id() const
{
    return repository_? repository_->id() : "";
}


bool
Conversation::addMember(const std::string& contactUri)
{
    // Retrieve certificate
    auto cert = tls::CertificateStore::instance().getCertificate(contactUri);
    if (!cert) {
        JAMI_WARN("Could not add member %s because no certificate is found", contactUri.c_str());
        return false;
    }
    // Add member files and commit
    return !repository_->addMember(cert).empty();
}

bool
Conversation::removeMember(const std::string& contactUri)
{
    return true;
}

std::vector<std::map<std::string, std::string>>
Conversation::getMembers()
{
    std::vector<std::map<std::string, std::string>> result;
    auto shared = account_.lock();
    if (!shared) return result;

    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository_->id();
    auto adminsPath = repoPath+DIR_SEPARATOR_STR+"admins";
    auto membersPath = repoPath+DIR_SEPARATOR_STR+"members";
    for (const auto& certificate: fileutils::readDirectory(adminsPath)) {
        if (certificate.find(".crt") == std::string::npos) {
            JAMI_WARN("Incorrect file found: %s/%s", adminsPath.c_str(), certificate.c_str());
            continue;
        }
        std::map<std::string, std::string> details {
            {"uri", certificate.substr(0,certificate.size()-std::string(".crt").size())},
            {"role","admin"}
        };
        result.emplace_back(details);
    }
    for (const auto& certificate: fileutils::readDirectory(membersPath)) {
        if (certificate.find(".crt") == std::string::npos) {
            JAMI_WARN("Incorrect file found: %s/%s", membersPath.c_str(), certificate.c_str());
            continue;
        }
        std::map<std::string, std::string> details {
            {"uri", certificate.substr(0,certificate.size()-std::string(".crt").size())},
            {"role","member"}
        };
        result.emplace_back(details);
    }

    return result;
}

bool
Conversation::isMember(const std::string& uri)
{
    auto shared = account_.lock();
    if (!shared) return false;

    auto repoPath = fileutils::get_data_dir()+DIR_SEPARATOR_STR+shared->getAccountID()+DIR_SEPARATOR_STR+"conversations"+DIR_SEPARATOR_STR+repository_->id();
    auto adminsPath = repoPath+DIR_SEPARATOR_STR+"admins";
    auto membersPath = repoPath+DIR_SEPARATOR_STR+"members";
    for (const auto& certificate: fileutils::readDirectory(adminsPath)) {
        if (certificate.find(".crt") == std::string::npos) {
            JAMI_WARN("Incorrect file found: %s/%s", adminsPath.c_str(), certificate.c_str());
            continue;
        }
        auto crtUri = certificate.substr(0,certificate.size()-std::string(".crt").size());
        if (crtUri == uri) return true;
    }
    for (const auto& certificate: fileutils::readDirectory(membersPath)) {
        if (certificate.find(".crt") == std::string::npos) {
            JAMI_WARN("Incorrect file found: %s/%s", membersPath.c_str(), certificate.c_str());
            continue;
        }
        auto crtUri = certificate.substr(0,certificate.size()-std::string(".crt").size());
        if (crtUri == uri) return true;
    }

    return false;
}

std::string
Conversation::sendMessage(const std::string& message, const std::string& type, const std::string& parent)
{
    Json::Value json;
    json["body"] = message;
    json["type"] = type;
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return repository_->commitMessage(Json::writeString(wbuilder, json));
}

std::vector<std::map<std::string, std::string>>
Conversation::loadMessages(const std::string& fromMessage, size_t n)
{
    if (!repository_) return {};
    auto convCommits = repository_->log(fromMessage, n);
    std::vector<std::map<std::string, std::string>> result;
    result.resize(n);
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
            if (!reader->parse(commit.commit_msg.data(), commit.commit_msg.data() + commit.commit_msg.size(), &cm, &err)) {
                type = cm["type"].asString();
                body = cm["body"].asString();
            }
        }
        std::map<std::string, std::string> message {
            {"id", commit.id},
            {"parents", parents},
            {"author", authorId},
            {"type", type},
            {"body", body},
            {"timestamp", std::to_string(commit.timestamp)}
        };
        result.emplace_back(message);
    }
    return result;
}

bool
Conversation::fetchFrom(const std::string& uri)
{
    // TODO check if device id or account id
    return repository_->fetch(uri);
}


}