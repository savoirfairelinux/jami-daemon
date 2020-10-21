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

void
togglePlugin(const std::string& path, bool toggle)
{
    jami::Manager::instance().getJamiPluginManager().togglePlugin(path, toggle);
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
listAvailablePlugins()
{
    return jami::Manager::instance().getJamiPluginManager().listAvailablePlugins();
}

std::vector<std::string>
listLoadedPlugins()
{
    return jami::Manager::instance().getJamiPluginManager().listLoadedPlugins();
}

int
installPlugin(const std::string& jplPath, bool force)
{
    return jami::Manager::instance().getJamiPluginManager().installPlugin(jplPath, force);
}

int
uninstallPlugin(const std::string& pluginRootPath)
{
    return jami::Manager::instance().getJamiPluginManager().uninstallPlugin(pluginRootPath);
}

std::vector<std::string>
listCallMediaHandlers()
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .listCallMediaHandlers();
}

void
toggleCallMediaHandler(const std::string& id, bool toggle)
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .toggleCallMediaHandler(id, toggle, jami::Manager::instance().getCurrentCallId());
}

std::map<std::string, std::string>
getCallMediaHandlerDetails(const std::string& id)
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .getCallMediaHandlerDetails(id);
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
    jami::Manager::instance().saveConfig();
}

std::map<std::string, std::string>
getCallMediaHandlerStatus()
{
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .getCallMediaHandlerStatus();
}
} // namespace DRing
