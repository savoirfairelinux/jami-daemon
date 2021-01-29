/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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
#pragma once

#include "noncopyable.h"
#include "pluginmanager.h"
#include "chathandler.h"
#include "pluginpreferencesutils.h"

namespace jami {

using ChatHandlerPtr = std::unique_ptr<ChatHandler>;

class ChatServicesManager
{
public:
    ChatServicesManager(PluginManager& pm);

    NON_COPYABLE(ChatServicesManager);

    void registerComponentsLifeCycleManagers(PluginManager& pm);

    void registerChatService(PluginManager& pm);

    std::vector<std::string> getChatHandlers();

    void publishMessage(pluginMessagePtr& cm);

    void cleanChatSubjects(const std::string& accountId, const std::string& peerId = "");

    void toggleChatHandler(const std::string& chatHandlerId,
                           const std::string& accountId,
                           const std::string& peerId,
                           const bool toggle);

    std::vector<std::string> getChatHandlerStatus(const std::string& accountId,
                                                  const std::string& peerId);

    /**
     * @brief getChatHandlerDetails
     * @param chatHandlerIdStr of the chat handler
     * @return map of Chat Handler Details
     */
    std::map<std::string, std::string> getChatHandlerDetails(const std::string& chatHandlerIdStr);

    bool setPreference(const std::string& key,
                       const std::string& value,
                       const std::string& rootPath);

private:
    void toggleChatHandler(const uintptr_t chatHandlerId,
                           const std::string& accountId,
                           const std::string& peerId,
                           const bool toggle);

    std::list<ChatHandlerPtr> chatHandlers_;
    std::map<std::pair<std::string, std::string>, std::set<uintptr_t>>
        chatHandlerToggled_; // {account,peer}, list of chatHandlers

    std::map<std::pair<std::string, std::string>, chatSubjectPtr> chatSubjects_;
    std::map<std::string, uintptr_t> handlersNameMap_ {};

    /// Component that stores persistent ChatHandlers' status for each existing
    /// accountId, peerId pair.
    /// A map of accountId, peerId pairs and ChatHandler-status pairs.
    ChatHandlerList allowDenyList_ {};
};
} // namespace jami
