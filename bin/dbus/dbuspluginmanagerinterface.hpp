/*
 *  Copyright (C) 2020-2023 Savoir-faire Linux Inc.
 *
 *  Author: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
 *  Author: Vladimir Stoiakin <vstoiakin@lavabit.com>
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

#include "dbuspluginmanagerinterface.adaptor.h"
#include <plugin_manager_interface.h>

class DBusPluginManagerInterface : public sdbus::AdaptorInterfaces<cx::ring::Ring::PluginManagerInterface_adaptor>
{
public:
    DBusPluginManagerInterface(sdbus::IConnection& connection)
        : AdaptorInterfaces(connection, "/cx/ring/Ring/PluginManagerInterface")
    {
        registerAdaptor();
        registerSignalHandlers();
    }

    ~DBusPluginManagerInterface()
    {
        unregisterAdaptor();
    }

    bool
    loadPlugin(const std::string& path)
    {
        return libjami::loadPlugin(path);
    }

    bool
    unloadPlugin(const std::string& path)
    {
        return libjami::unloadPlugin(path);
    }

    std::map<std::string, std::string>
    getPluginDetails(const std::string& path)
    {
        return libjami::getPluginDetails(path);
    }

    std::vector<std::map<std::string, std::string>>
    getPluginPreferences(const std::string& path,
                         const std::string& accountId)
    {
        return libjami::getPluginPreferences(path, accountId);
    }

    bool
    setPluginPreference(const std::string& path,
                        const std::string& accountId,
                        const std::string& key,
                        const std::string& value)
    {
        return libjami::setPluginPreference(path, accountId, key, value);
    }

    std::map<std::string, std::string>
    getPluginPreferencesValues(const std::string& path,
                               const std::string& accountId)
    {
        return libjami::getPluginPreferencesValues(path, accountId);
    }

    bool
    resetPluginPreferencesValues(const std::string& path,
                                 const std::string& accountId)
    {
        return libjami::resetPluginPreferencesValues(path, accountId);
    }

    std::map<std::string, std::string>
    getPlatformInfo()
    {
        return libjami::getPlatformInfo();
    }

    auto
    getInstalledPlugins() -> decltype(libjami::getInstalledPlugins())
    {
        return libjami::getInstalledPlugins();
    }

    auto
    getLoadedPlugins() -> decltype(libjami::getLoadedPlugins())
    {
        return libjami::getLoadedPlugins();
    }

    int
    installPlugin(const std::string& jplPath, const bool& force)
    {
        return libjami::installPlugin(jplPath, force);
    }

    int
    uninstallPlugin(const std::string& pluginRootPath)
    {
        return libjami::uninstallPlugin(pluginRootPath);
    }

    auto
    getCallMediaHandlers() -> decltype(libjami::getCallMediaHandlers())
    {
        return libjami::getCallMediaHandlers();
    }

    auto
    getChatHandlers() -> decltype(libjami::getChatHandlers())
    {
        return libjami::getChatHandlers();
    }
    void
    toggleCallMediaHandler(const std::string& mediaHandlerId,
                           const std::string& callId,
                           const bool& toggle)
    {
        libjami::toggleCallMediaHandler(mediaHandlerId, callId, toggle);
    }

    void
    toggleChatHandler(const std::string& chatHandlerId,
                      const std::string& accountId,
                      const std::string& peerId,
                      const bool& toggle)
    {
        libjami::toggleChatHandler(chatHandlerId, accountId, peerId, toggle);
    }

    std::map<std::string, std::string>
    getCallMediaHandlerDetails(const std::string& mediaHanlderId)
    {
        return libjami::getCallMediaHandlerDetails(mediaHanlderId);
    }

    std::vector<std::string>
    getCallMediaHandlerStatus(const std::string& callId)
    {
        return libjami::getCallMediaHandlerStatus(callId);
    }

    std::map<std::string, std::string>
    getChatHandlerDetails(const std::string& chatHanlderId)
    {
        return libjami::getChatHandlerDetails(chatHanlderId);
    }

    std::vector<std::string>
    getChatHandlerStatus(const std::string& accountId,
                         const std::string& peerId)
    {
        return libjami::getChatHandlerStatus(accountId, peerId);
    }

    bool
    getPluginsEnabled()
    {
        return libjami::getPluginsEnabled();
    }

    void
    setPluginsEnabled(const bool& state)
    {
        libjami::setPluginsEnabled(state);
    }

    void
    sendWebViewMessage(const std::string& pluginId,
                       const std::string& webViewId,
                       const std::string& messageId,
                       const std::string& payload)
    {
        libjami::sendWebViewAttach(pluginId, webViewId, messageId, payload);
    }

    std::string
    sendWebViewAttach(const std::string& pluginId,
                      const std::string& accountId,
                      const std::string& webViewId,
                      const std::string& action)
    {
        return libjami::sendWebViewAttach(pluginId, accountId, webViewId, action);
    }

    void
    sendWebViewDetach(const std::string& pluginId,
                      const std::string& webViewId)
    {
        libjami::sendWebViewDetach(pluginId, webViewId);
    }

private:

    void
    registerSignalHandlers()
    {
        using namespace std::placeholders;

        using libjami::exportable_serialized_callback;
        using SharedCallback = std::shared_ptr<libjami::CallbackWrapperBase>;

        const std::map<std::string, SharedCallback> pluginEvHandlers = {
            exportable_serialized_callback<libjami::PluginSignal::WebViewMessageReceived>(
                std::bind(&DBusPluginManagerInterface::emitWebViewMessageReceived, this, _1, _2, _3, _4)),
        };

        libjami::registerSignalHandlers(pluginEvHandlers);
    }

};
