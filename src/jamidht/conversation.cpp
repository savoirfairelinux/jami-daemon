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

#include "jamiaccount.h"
#include "conversationrepository.h"

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
    return repository_? "" : repository_->id();
}


void
Conversation::addMember(const std::string& contactUri)
{
    // Retrieve certificate
    auto cert = tls::CertificateStore::instance().getCertificate(contactUri);
    if (!cert) {
        JAMI_WARN("Could not add member %s because no certificate is found", contactUri.c_str());
        return;
    }
    auto parentCert = cert->issuer;
    if (!parentCert) {
        JAMI_WARN("Could not add member %s because no certificate is found", contactUri.c_str());
        return;
    }
    // Add member files and commit
    repository_->addMember(parentCert);
}

bool
Conversation::removeMember(const std::string& contactUri)
{
    return true;
}

std::vector<std::map<std::string, std::string>>
Conversation::getMembers()
{
    return {};
}

void
Conversation::sendMessage(const std::string& message, const std::string& parent)
{

}

void
Conversation::loadMessages(const std::string& fromMessage, size_t n)
{

}

}