/*
 *  Copyright (C) 2013-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "conversation_interface.h"

#include <cerrno>
#include <sstream>
#include <cstring>

#include "logger.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"

namespace DRing {

std::string
startConversation(const std::string& accountId) {
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->startConversation();
    return {};
}

bool
removeConversation(const std::string& accountId, const std::string& conversationId) {
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->removeConversation(conversationId);
    return false;
}

// Member management
void
addConversationMember(const std::string& accountId, const std::string& conversationId, const std::string& contactUri) {
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        acc->addConversationMember(conversationId, contactUri);
}

bool
removeConversationMember(const std::string& accountId, const std::string& conversationId, const std::string& contactUri) {
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->removeConversationMember(conversationId, contactUri);
    return false;
}

std::vector<std::map<std::string, std::string>>
getConversationMembers(const std::string& accountId, const std::string& conversationId) {
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        return acc->getConversationMembers(conversationId);
    return {};
}

// Message send/load
void
sendMessage(const std::string& accountId, const std::string& conversationId, const std::string& message, const std::string& parent) {
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        acc->sendMessage(conversationId, message, parent);
}

void
loadConversationMessages(const std::string& accountId, const std::string& conversationId, const std::string& fromMessage, size_t n) {
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        acc->loadConversationMessages(conversationId, fromMessage, n);
}

} // namespace DRing
