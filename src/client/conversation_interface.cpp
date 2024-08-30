/*
 *  Copyright (C) 2013-2024 Savoir-faire Linux Inc.
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
        if (auto convModule = acc->convModule(true))
            return convModule->startConversation();
    return {};
}

void
acceptConversationRequest(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
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
        if (auto convModule = acc->convModule(true))
            return convModule->removeConversation(conversationId);
    return false;
}

std::vector<std::string>
getConversations(const std::string& accountId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
            return convModule->getConversations();
    return {};
}

std::vector<std::map<std::string, std::string>>
getActiveCalls(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
            return convModule->getActiveCalls(conversationId);
    return {};
}

std::vector<std::map<std::string, std::string>>
getConversationRequests(const std::string& accountId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
            return convModule->getConversationRequests();
    return {};
}

void
updateConversationInfos(const std::string& accountId,
                        const std::string& conversationId,
                        const std::map<std::string, std::string>& infos)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
            convModule->updateConversationInfos(conversationId, infos);
}

std::map<std::string, std::string>
conversationInfos(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
            return convModule->conversationInfos(conversationId);
    return {};
}

void
setConversationPreferences(const std::string& accountId,
                           const std::string& conversationId,
                           const std::map<std::string, std::string>& prefs)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
            convModule->setConversationPreferences(conversationId, prefs);
}

std::map<std::string, std::string>
getConversationPreferences(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
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
        if (auto convModule = acc->convModule(true)) {
            dht::InfoHash h(contactUri);
            if (not h) {
                JAMI_ERROR("addConversationMember: invalid contact URI `{}`", contactUri);
                return;
            }
            convModule->addConversationMember(conversationId, h);
        }
}

void
removeConversationMember(const std::string& accountId,
                         const std::string& conversationId,
                         const std::string& contactUri)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true)) {
            dht::InfoHash h(contactUri);
            if (not h) {
                JAMI_ERROR("removeConversationMember: invalid contact URI `{}`", contactUri);
                return;
            }
            convModule->removeConversationMember(conversationId, h);
        }
}

std::vector<std::map<std::string, std::string>>
getConversationMembers(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
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
        if (auto convModule = acc->convModule(true)) {
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
        if (auto convModule = acc->convModule(true))
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
        if (auto convModule = acc->convModule(true))
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
        if (auto convModule = acc->convModule(true))
            return convModule->loadConversationUntil(conversationId, fromMessage, toMessage);
    return 0;
}

uint32_t
loadSwarmUntil(const std::string& accountId,
               const std::string& conversationId,
               const std::string& fromMessage,
               const std::string& toMessage)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
            return convModule->loadSwarmUntil(conversationId, fromMessage, toMessage);
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
        if (auto convModule = acc->convModule(true))
            return convModule->countInteractions(conversationId, toId, fromId, authorUri);
    return 0;
}

void
clearCache(const std::string& accountId, const std::string& conversationId)
{
    if (auto acc = jami::Manager::instance().getAccount<jami::JamiAccount>(accountId))
        if (auto convModule = acc->convModule(true))
            convModule->clearCache(conversationId);
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
            if (auto convModule = acc->convModule(true)) {
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
        acc->reloadContacts();
        if (auto convModule = acc->convModule(true)) {
            convModule->reloadRequests();
            convModule->loadConversations();
        }
    }
}

} // namespace libjami
