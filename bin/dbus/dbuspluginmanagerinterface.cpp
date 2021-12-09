/*
 *  Copyright (C)2020-2021 Savoir-faire Linux Inc.
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
#include "jami/plugin_manager_interface.h"

DBusPluginManagerInterface::DBusPluginManagerInterface(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/PluginManagerInterface")
{}

bool
DBusPluginManagerInterface::loadPlugin(const std::string& path)
{
    return DRing::loadPlugin(path);
}

bool
DBusPluginManagerInterface::unloadPlugin(const std::string& path)
{
    return DRing::unloadPlugin(path);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getPluginDetails(const std::string& path)
{
    return DRing::getPluginDetails(path);
}

std::vector<std::map<std::string, std::string>>
DBusPluginManagerInterface::getPluginPreferences(const std::string& path,
                                                 const std::string& accountId)
{
    return DRing::getPluginPreferences(path, accountId);
}

bool
DBusPluginManagerInterface::setPluginPreference(const std::string& path,
                                                const std::string& accountId,
                                                const std::string& key,
                                                const std::string& value)
{
    return DRing::setPluginPreference(path, accountId, key, value);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getPluginPreferencesValues(const std::string& path,
                                                       const std::string& accountId)
{
    return DRing::getPluginPreferencesValues(path, accountId);
}

bool
DBusPluginManagerInterface::resetPluginPreferencesValues(const std::string& path,
                                                         const std::string& accountId)
{
    return DRing::resetPluginPreferencesValues(path, accountId);
}

auto
DBusPluginManagerInterface::getInstalledPlugins() -> decltype(DRing::getInstalledPlugins())
{
    return DRing::getInstalledPlugins();
}

auto
DBusPluginManagerInterface::getLoadedPlugins() -> decltype(DRing::getLoadedPlugins())
{
    return DRing::getLoadedPlugins();
}

int
DBusPluginManagerInterface::installPlugin(const std::string& jplPath, const bool& force)
{
    return DRing::installPlugin(jplPath, force);
}

int
DBusPluginManagerInterface::uninstallPlugin(const std::string& pluginRootPath)
{
    return DRing::uninstallPlugin(pluginRootPath);
}

auto
DBusPluginManagerInterface::getCallMediaHandlers() -> decltype(DRing::getCallMediaHandlers())
{
    return DRing::getCallMediaHandlers();
}

auto
DBusPluginManagerInterface::getChatHandlers() -> decltype(DRing::getChatHandlers())
{
    return DRing::getChatHandlers();
}

void
DBusPluginManagerInterface::toggleCallMediaHandler(const std::string& mediaHandlerId,
                                                   const std::string& callId,
                                                   const bool& toggle)
{
    DRing::toggleCallMediaHandler(mediaHandlerId, callId, toggle);
}

void
DBusPluginManagerInterface::toggleChatHandler(const std::string& chatHandlerId,
                                              const std::string& accountId,
                                              const std::string& peerId,
                                              const bool& toggle)
{
    DRing::toggleChatHandler(chatHandlerId, accountId, peerId, toggle);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getCallMediaHandlerDetails(const std::string& mediaHanlderId)
{
    return DRing::getCallMediaHandlerDetails(mediaHanlderId);
}

std::vector<std::string>
DBusPluginManagerInterface::getCallMediaHandlerStatus(const std::string& callId)
{
    return DRing::getCallMediaHandlerStatus(callId);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getChatHandlerDetails(const std::string& chatHanlderId)
{
    return DRing::getChatHandlerDetails(chatHanlderId);
}

std::vector<std::string>
DBusPluginManagerInterface::getChatHandlerStatus(const std::string& accountId,
                                                 const std::string& peerId)
{
    return DRing::getChatHandlerStatus(accountId, peerId);
}

bool
DBusPluginManagerInterface::getPluginsEnabled()
{
    return DRing::getPluginsEnabled();
}

void
DBusPluginManagerInterface::setPluginsEnabled(const bool& state)
{
    DRing::setPluginsEnabled(state);
}
