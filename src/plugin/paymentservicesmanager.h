/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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
//#include "paymenthandler.h"
#include "pluginpreferencesutils.h"

namespace jami {

class PluginManager;

//using PaymentHandlerPtr = std::unique_ptr<PaymentHandler>;

/**
 * @brief This class provides the interface between loaded PaymentHandlers
 * and conversation messages. Besides it:
 * (1) stores pointers to all loaded PaymentHandlers;
 * (2) stores pointers to availables Payment subjects, and;
 * (3) lists PaymentHandler state with respect to each accountId. In other words,
 * for a given accountId, we store the PaymentHandler properties.
 */
class PaymentServicesManager
{
public:
    /**
     * @brief Constructor registers PaymentHandler API services to the PluginManager
     * instance. These services will store PaymentHandler pointers, clean them
     * from the Plugin System once a plugin is loaded or unloaded, or yet allows
     * the plugins to send a message to a conversation.
     * @param pluginManager
     */
    PaymentServicesManager(PluginManager& pluginManager);

    NON_COPYABLE(PaymentServicesManager);

    /**
     * @brief List all PaymentHandlers available.
     * @return Vector of stored PaymentHandlers pointers.
     */
    std::vector<std::string> getPaymentHandlers();

    /**
     * @brief If an account is unregistered, we clear all payment subjects
     * related to that accountId.
     * @param accountId
     */
    void cleanPaymentSubjects(const std::string& accountId);

    /**
     * @brief Activates or deactivate a given PaymentHandler to a given accountId.
     * @param PaymentHandlerId
     * @param accountId
     * @param toggle Notify with new subjects if true, detach if false.
     */
    void togglePaymentHandler(const std::string& chatHandlerId,
                           const std::string& accountId,
                           const bool toggle);

    /**
     * @brief Returns a list of active PaymentHandlers for a given accountId.
     * @param accountId
     * @return Vector with active PaymentHandler ids for a given accountId.
     */
    std::vector<std::string> getPaymentHandlerStatus(const std::string& accountId);

    /**
     * @brief Gets details from PaymentHandler implementation.
     * @param paymentHandlerIdStr
     * @return Details map from the PaymentHandler implementation
     */
    std::map<std::string, std::string> getPaymentHandlerDetails(const std::string& paymentHandlerIdStr);

    /**
     * @brief Sets a preference that may be changed while PaymentHandler is active.
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
     * @brief Exposes PaymentHandlers' life cycle managers services to the main API.
     * @param pluginManager
     */
    void registerComponentsLifeCycleManagers(PluginManager& pluginManager);

    /**
     * @brief Exposes PaymentHandlers services that aren't related to handlers' life cycle
     * to the main API.
     * @param pluginManager
     */
    void registerPaymentService(PluginManager& pluginManager);

    void togglePaymentHandler(const uintptr_t chatHandlerId,
                           const std::string& accountId,
                           const bool toggle);

    // // Components that a plugin can register through registerChatHandler service.
    // // These objects can then be activated with toggleChatHandler.
    // std::list<ChatHandlerPtr> chatHandlers_;

    // // Component that stores active ChatHandlers for each existing accountId, peerId pair.
    // std::map<std::pair<std::string, std::string>, std::set<uintptr_t>> chatHandlerToggled_;

    // // When there is a new message, chat subjects are created.
    // // Here we store a reference to them in order to make them interact with
    // // ChatHandlers.
    // // For easy access they are mapped accordingly to the accountId, peerId pair to
    // // which they belong.
    // std::map<std::pair<std::string, std::string>, chatSubjectPtr> chatSubjects_;

    // // Maps a ChatHandler name and the address of this ChatHandler.
    // std::map<std::string, uintptr_t> handlersNameMap_ {};

    // // Component that stores persistent ChatHandlers' status for each existing
    // // accountId, peerId pair.
    // // A map of accountId, peerId pairs and ChatHandler-status pairs.
    // ChatHandlerList allowDenyList_ {};
};
} // namespace jami
