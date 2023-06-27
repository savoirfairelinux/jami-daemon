/*
 *  Copyright (C) 2013-2023 Savoir-faire Linux Inc.
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
#include "jamidht/conversation_module.h"

namespace libjami {

std::string
startConversation(const std::string& accountId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->startConversation();
    return {};
}

void
acceptConversationRequest(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            convModule->acceptConversationRequest(conversationId);
}

void
declineConversationRequest(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        acc->declineConversationRequest(conversationId);
}

bool
removeConversation(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->removeConversation(conversationId);
    return false;
}

std::vector<std::string>
getConversations(const std::string& accountId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->getConversations();
    return {};
}

std::vector<std::map<std::string, std::string>>
getActiveCalls(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->getActiveCalls(conversationId);
    return {};
}

std::vector<std::map<std::string, std::string>>
getConversationRequests(const std::string& accountId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->getConversationRequests();
    return {};
}

void
updateConversationInfos(const std::string& accountId,
                        const std::string& conversationId,
                        const std::map<std::string, std::string>& infos)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            convModule->updateConversationInfos(conversationId, infos);
}

std::map<std::string, std::string>
conversationInfos(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->conversationInfos(conversationId);
    return {};
}

void
setConversationPreferences(const std::string& accountId,
                           const std::string& conversationId,
                           const std::map<std::string, std::string>& prefs)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            convModule->setConversationPreferences(conversationId, prefs);
}

std::map<std::string, std::string>
getConversationPreferences(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->getConversationPreferences(conversationId);
    return {};
}

// Member management
void
addConversationMember(const std::string& accountId,
                      const std::string& conversationId,
                      const std::string& contactUri)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            convModule->addConversationMember(conversationId, contactUri);
}

void
removeConversationMember(const std::string& accountId,
                         const std::string& conversationId,
                         const std::string& contactUri)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            convModule->removeConversationMember(conversationId, contactUri);
}

std::vector<std::map<std::string, std::string>>
getConversationMembers(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->getConversationMembers(conversationId, true);
    return {};
}

// Message send/load
void
sendMessage(const std::string& accountId,
            const std::string& conversationId,
            const std::string& message,
            const std::string& commitId,
            const int32_t& flag)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule()) {
            if (flag == 0 /* Reply or simple commit */) {
                convModule->sendMessage(conversationId, message, commitId);
            } else if (flag == 1 /* message edition */) {
                convModule->editMessage(conversationId, message, commitId);
            } else if (flag == 2 /* reaction */) {
                convModule->reactToMessage(conversationId, message, commitId);
            }
        }
}

uint32_t
loadConversationMessages(const std::string& accountId,
                         const std::string& conversationId,
                         const std::string& fromMessage,
                         size_t n)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->loadConversationMessages(conversationId, fromMessage, n);
    return 0;
}

uint32_t
loadConversation(const std::string& accountId,
                         const std::string& conversationId,
                         const std::string& fromMessage,
                         size_t n)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->loadConversation(conversationId, fromMessage, n);
    return 0;
}

uint32_t
loadConversationUntil(const std::string& accountId,
                      const std::string& conversationId,
                      const std::string& fromMessage,
                      const std::string& toMessage)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->loadConversationUntil(conversationId, fromMessage, toMessage);
    return 0;
}

uint32_t
countInteractions(const std::string& accountId,
                  const std::string& conversationId,
                  const std::string& toId,
                  const std::string& fromId,
                  const std::string& authorUri)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule())
            return convModule->countInteractions(conversationId, toId, fromId, authorUri);
    return 0;
}

uint32_t
searchConversation(const std::string& accountId,
                   const std::string& conversationId,
                   const std::string& author,
                   const std::string& lastId,
                   const std::string& regexSearch,
                   const std::string& type,
                   const int64_t& after,
                   const int64_t& before,
                   const uint32_t& maxResult,
                   const int32_t& flag)
{
    uint32_t res = 0;
    jami::Filter filter {author, lastId, regexSearch, type, after, before, maxResult, flag != 0};
    for (const auto& accId : jami::Manager::instance().getAccountList()) {
        if (!accountId.empty() && accId != accountId)
            continue;
        if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accId)) {
            res = std::uniform_int_distribution<uint32_t>()(acc->rand);
            if (auto convModule = acc->convModule()) {
                convModule->search(res, conversationId, filter);
            }
        }
    }
    return res;
}

void
reloadConversationsAndRequests(const std::string& accountId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId)) {
        if (auto convModule = acc->convModule()) {
            convModule->reloadRequests();
            convModule->loadConversations();
        }
    }
}

} // namespace libjami
