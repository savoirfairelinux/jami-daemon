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

#include "plugin_manager_interface.h"
#include "manager.h"
#include "plugin/jamipluginmanager.h"
#include "logger.h"
#include <iostream>

namespace DRing {
bool
loadPlugin(const std::string& path)
{
    bool status = jami::Manager::instance().getJamiPluginManager().loadPlugin(path);

    jami::Manager::instance().pluginPreferences.saveStateLoadedPlugins(path, status);
    jami::Manager::instance().saveConfig();
    return status;
}

bool
unloadPlugin(const std::string& path)
{
    bool status = jami::Manager::instance().getJamiPluginManager().unloadPlugin(path);

    jami::Manager::instance().pluginPreferences.saveStateLoadedPlugins(path, false);
    jami::Manager::instance().saveConfig();
    return status;
}

std::map<std::string, std::string>
getPluginDetails(const std::string& path)
{
    return jami::Manager::instance().getJamiPluginManager().getPluginDetails(path);
}

std::vector<std::map<std::string, std::string>>
getPluginPreferences(const std::string& path)
{
    return jami::Manager::instance().getJamiPluginManager().getPluginPreferences(path);
}

bool
setPluginPreference(const std::string& path, const std::string& key, const std::string& value)
{
    return jami::Manager::instance().getJamiPluginManager().setPluginPreference(path, key, value);
}

std::map<std::string, std::string>
getPluginPreferencesValues(const std::string& path)
{
    return jami::Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(path);
}
bool
resetPluginPreferencesValues(const std::string& path)
{
    return jami::Manager::instance().getJamiPluginManager().resetPluginPreferencesValuesMap(path);
}

std::vector<std::string>
getInstalledPlugins()
{
    return jami::Manager::instance().getJamiPluginManager().getInstalledPlugins();
}

std::vector<std::string>
getLoadedPlugins()
{
    return jami::Manager::instance().pluginPreferences.getLoadedPlugins();
}

int
installPlugin(const std::string& jplPath, bool force)
{
    return jami::Manager::instance().getJamiPluginManager().installPlugin(jplPath, force);
}

int
uninstallPlugin(const std::string& pluginRootPath)
{
    int status = jami::Manager::instance().getJamiPluginManager().uninstallPlugin(pluginRootPath);
    jami::Manager::instance().pluginPreferences.saveStateLoadedPlugins(pluginRootPath, false);
    jami::Manager::instance().saveConfig();
    return status;
}

std::vector<std::string>
getCallMediaHandlers()
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .getCallMediaHandlers();
}

std::vector<std::string>
getChatHandlers()
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getChatServicesManager()
        .getChatHandlers();
}

void
toggleCallMediaHandler(const std::string& mediaHandlerId, const std::string& callId, bool toggle)
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .toggleCallMediaHandler(mediaHandlerId, callId, toggle);
}

void
toggleChatHandler(const std::string& chatHandlerId,
                  const std::string& accountId,
                  const std::string& peerId,
                  bool toggle)
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getChatServicesManager()
        .toggleChatHandler(chatHandlerId, accountId, peerId, toggle);
}

std::map<std::string, std::string>
getCallMediaHandlerDetails(const std::string& mediaHandlerId)
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .getCallMediaHandlerDetails(mediaHandlerId);
}

std::vector<std::string>
getCallMediaHandlerStatus(const std::string& callId)
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .getCallMediaHandlerStatus(callId);
}

std::map<std::string, std::string>
getChatHandlerDetails(const std::string& chatHandlerId)
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getChatServicesManager()
        .getChatHandlerDetails(chatHandlerId);
}

std::vector<std::string>
getChatHandlerStatus(const std::string& accountId, const std::string& peerId)
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getChatServicesManager()
        .getChatHandlerStatus(accountId, peerId);
}

bool
getPluginsEnabled()
{
    return jami::Manager::instance().pluginPreferences.getPluginsEnabled();
}

void
setPluginsEnabled(bool state)
{
    jami::Manager::instance().pluginPreferences.setPluginsEnabled(state);
    for (auto& item : jami::Manager::instance().pluginPreferences.getLoadedPlugins()) {
        if (state)
            jami::Manager::instance().getJamiPluginManager().loadPlugin(item);
        else
            jami::Manager::instance().getJamiPluginManager().unloadPlugin(item);
    }
    jami::Manager::instance().saveConfig();
}
} // namespace DRing
