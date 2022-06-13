/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
 *
 *  Author: Tobias Hildebrandt <tobias.hildebrandt@savoirfairelinux.com>
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
#include "webviewhandler.h"
#include "pluginpreferencesutils.h"
#include <list>

namespace jami {

class PluginManager;

using WebViewHandlerPtr = std::unique_ptr<WebViewHandler>;

/**
 * @brief This class provides the interface between loaded WebViewHandlers
 * and client webviews.
 */
class WebViewServicesManager
{
public:
    /**
     * @brief Constructor registers WebViewHandler API services to the PluginManager
     * instance. These services will store WebViewHandler pointers, clean them
     * from the Plugin System once a plugin is loaded or unloaded, or yet allows
     * the plugins to interact with webviews.
     * @param pluginManager
     */
    WebViewServicesManager(PluginManager& pluginManager);

    NON_COPYABLE(WebViewServicesManager);

    /**
     * @brief Transmits a message from the client's webview to the plugin
     * @param pluginId
     * @param webViewId
     * @param messageId
     * @param payload The message itself
     */
    void sendWebViewMessage(const std::string& pluginId,
                            const std::string& webViewId,
                            const std::string& messageId,
                            const std::string& payload);

    /**
     * @brief Transmits an attach event from the client's webview to the plugin
     * @param pluginId
     * @param accountId
     * @param webViewId
     * @param action The reason why the webview was created
     */
    std::string sendWebViewAttach(const std::string& pluginId,
                                  const std::string& accountId,
                                  const std::string& webViewId,
                                  const std::string& action);

    /**
     * @brief Transmits a detach event from the client's webview to the plugin
     * @param pluginId
     * @param accountId
     * @param webViewId
     */
    void sendWebViewDetach(const std::string& pluginId,
                           const std::string& accountId,
                           const std::string& webViewId);

    /**
     * @brief Get all registered webview handlers
     * @return Vector of handler addresses as strings
     */
    std::vector<std::string> getWebViewHandlers() const;

    /**
     * @brief Gets details from WebViewHandler implementation.
     * @param pluginId the pointer address of the webview handler
     * @return Details map from the WebViewHandler implementation
     */
    std::map<std::string, std::string> getWebViewHandlerDetails(const std::string& handlerAddress);

private:
    /**
     * @brief Exposes WebViewHandler' life cycle managers services to the main API.
     * @param pluginManager
     */
    void registerComponentsLifeCycleManagers(PluginManager& pluginManager);

    /**
     * @brief Exposes WebViewHandler services that aren't related to handlers' life cycle to the
     * main API.
     * @param pluginManager
     */
    void registerWebViewService(PluginManager& pluginManager);

    /**
     * @brief Get the webview handler for a specified plugin
     * @return The webview handler for the specified plugin
     */
    WebViewHandler* getWebViewHandlerPointer(const std::string& pluginId);

    /**
     * @brief map of all registered handlers (even ones inactive for all accounts), pluginId ->
     * HandlerPtr
     */
    std::map<std::string, WebViewHandlerPtr> handlersIdMap {};
};
} // namespace jami
