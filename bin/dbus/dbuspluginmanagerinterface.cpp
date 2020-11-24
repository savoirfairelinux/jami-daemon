/*
 *  Copyright (C) 2020 Savoir-faire Linux Inc.
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
#include "dring/plugin_manager_interface.h"

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

void
DBusPluginManagerInterface::togglePlugin(const std::string& path, const bool& toggle)
{
    DRing::togglePlugin(path, toggle);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getPluginDetails(const std::string& path)
{
    return DRing::getPluginDetails(path);
}

std::vector<std::map<std::string, std::string>>
DBusPluginManagerInterface::getPluginPreferences(const std::string& path)
{
    return DRing::getPluginPreferences(path);
}

bool
DBusPluginManagerInterface::setPluginPreference(const std::string& path,
                                                const std::string& key,
                                                const std::string& value)
{
    return DRing::setPluginPreference(path, key, value);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getPluginPreferencesValues(const std::string& path)
{
    return DRing::getPluginPreferencesValues(path);
}

bool
DBusPluginManagerInterface::resetPluginPreferencesValues(const std::string& path)
{
    return DRing::resetPluginPreferencesValues(path);
}

auto
DBusPluginManagerInterface::listAvailablePlugins() -> decltype(DRing::listAvailablePlugins())
{
    return DRing::listAvailablePlugins();
}

auto
DBusPluginManagerInterface::listLoadedPlugins() -> decltype(DRing::listLoadedPlugins())
{
    return DRing::listLoadedPlugins();
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
DBusPluginManagerInterface::listCallMediaHandlers() -> decltype(DRing::listCallMediaHandlers())
{
    return DRing::listCallMediaHandlers();
}

void
DBusPluginManagerInterface::toggleCallMediaHandler(const std::string& id, const bool& toggle)
{
    DRing::toggleCallMediaHandler(id, toggle);
}

std::map<std::string, std::string>
DBusPluginManagerInterface::getCallMediaHandlerDetails(const std::string& id)
{
    return DRing::getCallMediaHandlerDetails(id);
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

std::map<std::string, std::string>
DBusPluginManagerInterface::getCallMediaHandlerStatus()
{
    return DRing::getCallMediaHandlerStatus();
}
