/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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
#include "chathandler.h"
#include "pluginpreferencesutils.h"

namespace jami {

class PluginManager;

using ChatHandlerPtr = std::unique_ptr<ChatHandler>;

/**
 * @brief This class provides the interface between loaded ChatHandlers
 * and conversation messages. Besides it:
 * (1) stores pointers to all loaded ChatHandlers;
 * (2) stores pointers to availables chat subjects, and;
 * (3) lists ChatHandler state with respect to each accountId, peerId pair. In other words,
 * for a given accountId, peerId pair, we store if a ChatHandler is active or not.
 */
class ChatServicesManager
{
public:
    /**
     * @brief Constructor registers ChatHandler API services to the PluginManager
     * instance. These services will store ChatHandler pointers, clean them
     * from the Plugin System once a plugin is loaded or unloaded, or yet allows
     * the plugins to send a message to a conversation.
     * @param pluginManager
     */
    ChatServicesManager(PluginManager& pluginManager);

    NON_COPYABLE(ChatServicesManager);

    bool hasHandlers() const;

    /**
     * @brief List all ChatHandlers available.
     * @return Vector of stored ChatHandlers pointers.
     */
    std::vector<std::string> getChatHandlers() const;

    /**
     * @brief Publishes every message sent or received in a conversation that has (or should have)
     * an active ChatHandler.
     * @param message
     */
    void publishMessage(pluginMessagePtr message);

    /**
     * @brief If an account is unregistered or a contact is erased, we clear all chat subjects
     * related to that accountId or to the accountId, peerId pair.
     * @param accountId
     * @param peerId
     */
    void cleanChatSubjects(const std::string& accountId, const std::string& peerId = "");

    /**
     * @brief Activates or deactivate a given ChatHandler to a given accountId, peerId pair.
     * @param ChatHandlerId
     * @param accountId
     * @param peerId
     * @param toggle Notify with new subjects if true, detach if false.
     */
    void toggleChatHandler(const std::string& chatHandlerId,
                           const std::string& accountId,
                           const std::string& peerId,
                           const bool toggle);

    /**
     * @brief Returns a list of active ChatHandlers for a given accountId, peerId pair.
     * @param accountId
     * @param peerId
     * @return Vector with active ChatHandler ids for a given accountId, peerId pair.
     */
    std::vector<std::string> getChatHandlerStatus(const std::string& accountId,
                                                  const std::string& peerId);

    /**
     * @brief Gets details from ChatHandler implementation.
     * @param chatHandlerIdStr
     * @return Details map from the ChatHandler implementation
     */
    std::map<std::string, std::string> getChatHandlerDetails(const std::string& chatHandlerIdStr);

    /**
     * @brief Sets a preference that may be changed while ChatHandler is active.
     * @param key
     * @param value
     * @param rootPath
     * @return False if preference was changed.
     */
    bool setPreference(const std::string& key,
                       const std::string& value,
                       const std::string& rootPath);

private:
    /**
     * @brief Exposes ChatHandlers' life cycle managers services to the main API.
     * @param pluginManager
     */
    void registerComponentsLifeCycleManagers(PluginManager& pluginManager);

    /**
     * @brief Exposes ChatHandlers services that aren't related to handlers' life cycle
     * to the main API.
     * @param pluginManager
     */
    void registerChatService(PluginManager& pluginManager);

    void toggleChatHandler(const uintptr_t chatHandlerId,
                           const std::string& accountId,
                           const std::string& peerId,
                           const bool toggle);

    // Components that a plugin can register through registerChatHandler service.
    // These objects can then be activated with toggleChatHandler.
    std::list<ChatHandlerPtr> chatHandlers_;

    // Component that stores active ChatHandlers for each existing accountId, peerId pair.
    std::map<std::pair<std::string, std::string>, std::set<uintptr_t>> chatHandlerToggled_;

    // When there is a new message, chat subjects are created.
    // Here we store a reference to them in order to make them interact with
    // ChatHandlers.
    // For easy access they are mapped accordingly to the accountId, peerId pair to
    // which they belong.
    std::map<std::pair<std::string, std::string>, chatSubjectPtr> chatSubjects_;

    // Maps a ChatHandler name and the address of this ChatHandler.
    std::map<std::string, uintptr_t> handlersNameMap_ {};

    // Component that stores persistent ChatHandlers' status for each existing
    // accountId, peerId pair.
    // A map of accountId, peerId pairs and ChatHandler-status pairs.
    ChatHandlerList allowDenyList_ {};
};
} // namespace jami
