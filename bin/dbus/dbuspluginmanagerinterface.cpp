/*
 *  Copyright (C) 2020-2023 Savoir-faire Linux Inc.
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

#include "dbuspluginmanagerinterface.h"
#include "plugin_manager_interface.h"

DBusPluginManagerInterface::DBusPluginManagerInterface(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/PluginManagerInterface")
{}

bool
DBusPluginManagerInterface::loadPlugin(const std::string& path)
{
    return libjami::loadPlugin(path);
}

bool
DBusPluginManagerInterface::unloadPlugin(const std::string& path)
{
    return libjami::unloadPlugin(path);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getPluginDetails(const std::string& path)
{
    return libjami::getPluginDetails(path);
}

std::vector<std::map<std::string, std::string>>
DBusPluginManagerInterface::getPluginPreferences(const std::string& path,
                                                 const std::string& accountId)
{
    return libjami::getPluginPreferences(path, accountId);
}

bool
DBusPluginManagerInterface::setPluginPreference(const std::string& path,
                                                const std::string& accountId,
                                                const std::string& key,
                                                const std::string& value)
{
    return libjami::setPluginPreference(path, accountId, key, value);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getPluginPreferencesValues(const std::string& path,
                                                       const std::string& accountId)
{
    return libjami::getPluginPreferencesValues(path, accountId);
}

bool
DBusPluginManagerInterface::resetPluginPreferencesValues(const std::string& path,
                                                         const std::string& accountId)
{
    return libjami::resetPluginPreferencesValues(path, accountId);
}

auto
DBusPluginManagerInterface::getInstalledPlugins() -> decltype(libjami::getInstalledPlugins())
{
    return libjami::getInstalledPlugins();
}

auto
DBusPluginManagerInterface::getLoadedPlugins() -> decltype(libjami::getLoadedPlugins())
{
    return libjami::getLoadedPlugins();
}

int
DBusPluginManagerInterface::installPlugin(const std::string& jplPath, const bool& force)
{
    return libjami::installPlugin(jplPath, force);
}

int
DBusPluginManagerInterface::uninstallPlugin(const std::string& pluginRootPath)
{
    return libjami::uninstallPlugin(pluginRootPath);
}

auto
DBusPluginManagerInterface::getCallMediaHandlers() -> decltype(libjami::getCallMediaHandlers())
{
    return libjami::getCallMediaHandlers();
}

auto
DBusPluginManagerInterface::getChatHandlers() -> decltype(libjami::getChatHandlers())
{
    return libjami::getChatHandlers();
}
void
DBusPluginManagerInterface::toggleCallMediaHandler(const std::string& mediaHandlerId,
                                                   const std::string& callId,
                                                   const bool& toggle)
{
    libjami::toggleCallMediaHandler(mediaHandlerId, callId, toggle);
}

void
DBusPluginManagerInterface::toggleChatHandler(const std::string& chatHandlerId,
                                              const std::string& accountId,
                                              const std::string& peerId,
                                              const bool& toggle)
{
    libjami::toggleChatHandler(chatHandlerId, accountId, peerId, toggle);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getCallMediaHandlerDetails(const std::string& mediaHanlderId)
{
    return libjami::getCallMediaHandlerDetails(mediaHanlderId);
}

std::vector<std::string>
DBusPluginManagerInterface::getCallMediaHandlerStatus(const std::string& callId)
{
    return libjami::getCallMediaHandlerStatus(callId);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getChatHandlerDetails(const std::string& chatHanlderId)
{
    return libjami::getChatHandlerDetails(chatHanlderId);
}

std::vector<std::string>
DBusPluginManagerInterface::getChatHandlerStatus(const std::string& accountId,
                                                 const std::string& peerId)
{
    return libjami::getChatHandlerStatus(accountId, peerId);
}

bool
DBusPluginManagerInterface::getPluginsEnabled()
{
    return libjami::getPluginsEnabled();
}

void
DBusPluginManagerInterface::setPluginsEnabled(const bool& state)
{
    libjami::setPluginsEnabled(state);
}

void
DBusPluginManagerInterface::sendWebViewMessage(const std::string& pluginId,
                                               const std::string& webViewId,
                                               const std::string& messageId,
                                               const std::string& payload)
{
    libjami::sendWebViewAttach(pluginId, webViewId, messageId, payload);
}

std::string
DBusPluginManagerInterface::sendWebViewAttach(const std::string& pluginId,
                                              const std::string& accountId,
                                              const std::string& webViewId,
                                              const std::string& action)
{
    return libjami::sendWebViewAttach(pluginId, accountId, webViewId, action);
}

void
DBusPluginManagerInterface::sendWebViewDetach(const std::string& pluginId,
                                              const std::string& webViewId)
{
    libjami::sendWebViewDetach(pluginId, webViewId);
}
