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

#include "plugin_manager_interface.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#ifdef ENABLE_PLUGIN
#include "manager.h"
#include "plugin/jamipluginmanager.h"
#endif

namespace libjami {
bool
loadPlugin(const std::string& path)
{
#ifdef ENABLE_PLUGIN
    bool status = jami::Manager::instance().getJamiPluginManager().loadPlugin(path);

    jami::Manager::instance().pluginPreferences.saveStateLoadedPlugins(path, status);
    jami::Manager::instance().saveConfig();
    return status;
#endif
    return false;
}

bool
unloadPlugin(const std::string& path)
{
#ifdef ENABLE_PLUGIN
    bool status = jami::Manager::instance().getJamiPluginManager().unloadPlugin(path);

    jami::Manager::instance().pluginPreferences.saveStateLoadedPlugins(path, false);
    jami::Manager::instance().saveConfig();
    return status;
#endif
    return false;
}

std::map<std::string, std::string>
getPluginDetails(const std::string& path)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance().getJamiPluginManager().getPluginDetails(path);
#endif
    return {};
}

std::vector<std::map<std::string, std::string>>
getPluginPreferences(const std::string& path, const std::string& accountId)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance().getJamiPluginManager().getPluginPreferences(path, accountId);
#endif
    return {};
}

bool
setPluginPreference(const std::string& path,
                    const std::string& accountId,
                    const std::string& key,
                    const std::string& value)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance().getJamiPluginManager().setPluginPreference(path,
                                                                                accountId,
                                                                                key,
                                                                                value);
#endif
    return {};
}

std::map<std::string, std::string>
getPluginPreferencesValues(const std::string& path, const std::string& accountId)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance().getJamiPluginManager().getPluginPreferencesValuesMap(path,
                                                                                          accountId);
#endif
    return {};
}
bool
resetPluginPreferencesValues(const std::string& path, const std::string& accountId)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance()
        .getJamiPluginManager()
        .resetPluginPreferencesValuesMap(path, accountId);
#endif
}

std::vector<std::string>
getInstalledPlugins()
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance().getJamiPluginManager().getInstalledPlugins();
#endif
    return {};
}

std::vector<std::string>
getLoadedPlugins()
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance().pluginPreferences.getLoadedPlugins();
#endif
    return {};
}

int
installPlugin(const std::string& jplPath, bool force)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance().getJamiPluginManager().installPlugin(jplPath, force);
#endif
    return -1;
}

int
uninstallPlugin(const std::string& pluginRootPath)
{
#ifdef ENABLE_PLUGIN
    int status = jami::Manager::instance().getJamiPluginManager().uninstallPlugin(pluginRootPath);
    jami::Manager::instance().pluginPreferences.saveStateLoadedPlugins(pluginRootPath, false);
    jami::Manager::instance().saveConfig();
    return status;
#endif
    return -1;
}

std::vector<std::string>
getCallMediaHandlers()
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .getCallMediaHandlers();
#endif
    return {};
}

std::vector<std::string>
getChatHandlers()
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getChatServicesManager()
        .getChatHandlers();
#endif
    return {};
}

void
toggleCallMediaHandler(const std::string& mediaHandlerId, const std::string& callId, bool toggle)
{
#ifdef ENABLE_PLUGIN
    jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .toggleCallMediaHandler(mediaHandlerId, callId, toggle);
#endif
}

void
toggleChatHandler(const std::string& chatHandlerId,
                  const std::string& accountId,
                  const std::string& peerId,
                  bool toggle)
{
#ifdef ENABLE_PLUGIN
    jami::Manager::instance()
        .getJamiPluginManager()
        .getChatServicesManager()
        .toggleChatHandler(chatHandlerId, accountId, peerId, toggle);
#endif
}

std::map<std::string, std::string>
getCallMediaHandlerDetails(const std::string& mediaHandlerId)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .getCallMediaHandlerDetails(mediaHandlerId);
#endif
    return {};
}

std::vector<std::string>
getCallMediaHandlerStatus(const std::string& callId)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getCallServicesManager()
        .getCallMediaHandlerStatus(callId);
#endif
    return {};
}

std::map<std::string, std::string>
getChatHandlerDetails(const std::string& chatHandlerId)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getChatServicesManager()
        .getChatHandlerDetails(chatHandlerId);
#endif
    return {};
}

std::vector<std::string>
getChatHandlerStatus(const std::string& accountId, const std::string& peerId)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getChatServicesManager()
        .getChatHandlerStatus(accountId, peerId);
#endif
    return {};
}

bool
getPluginsEnabled()
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance().pluginPreferences.getPluginsEnabled();
#endif
    return false;
}

void
setPluginsEnabled(bool state)
{
#ifdef ENABLE_PLUGIN
    jami::Manager::instance().pluginPreferences.setPluginsEnabled(state);
    for (auto& item : jami::Manager::instance().pluginPreferences.getLoadedPlugins()) {
        if (state)
            jami::Manager::instance().getJamiPluginManager().loadPlugin(item);
        else
            jami::Manager::instance().getJamiPluginManager().unloadPlugin(item);
    }
    jami::Manager::instance().saveConfig();
#endif
}

void
sendWebViewMessage(const std::string& pluginId,
                   const std::string& webViewId,
                   const std::string& messageId,
                   const std::string& payload)
{
#ifdef ENABLE_PLUGIN
    jami::Manager::instance()
        .getJamiPluginManager()
        .getWebViewServicesManager()
        .sendWebViewMessage(pluginId, webViewId, messageId, payload);
#endif
}

std::string
sendWebViewAttach(const std::string& pluginId,
                  const std::string& accountId,
                  const std::string& webViewId,
                  const std::string& action)
{
#ifdef ENABLE_PLUGIN
    return jami::Manager::instance()
        .getJamiPluginManager()
        .getWebViewServicesManager()
        .sendWebViewAttach(pluginId, accountId, webViewId, action);
#endif
}

void
sendWebViewDetach(const std::string& pluginId, const std::string& webViewId)
{
#ifdef ENABLE_PLUGIN
    jami::Manager::instance()
        .getJamiPluginManager()
        .getWebViewServicesManager()
        .sendWebViewDetach(pluginId, webViewId);
#endif
}
} // namespace libjami
