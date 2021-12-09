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

#pragma once

#include "jami.h"

#include "def.h"

#include <string>
#include <vector>
#include <map>
#include <list>

#if __APPLE__
#import "TargetConditionals.h"
#endif

namespace DRing {
DRING_PUBLIC bool loadPlugin(const std::string& path);
DRING_PUBLIC bool unloadPlugin(const std::string& path);
DRING_PUBLIC std::map<std::string, std::string> getPluginDetails(const std::string& path);
DRING_PUBLIC std::vector<std::map<std::string, std::string>> getPluginPreferences(
    const std::string& path, const std::string& accountId);
DRING_PUBLIC bool setPluginPreference(const std::string& path,
                                      const std::string& accountId,
                                      const std::string& key,
                                      const std::string& value);
DRING_PUBLIC std::map<std::string, std::string> getPluginPreferencesValues(
    const std::string& path, const std::string& accountId);
DRING_PUBLIC bool resetPluginPreferencesValues(const std::string& path,
                                               const std::string& accountId);
DRING_PUBLIC std::vector<std::string> getInstalledPlugins();
DRING_PUBLIC std::vector<std::string> getLoadedPlugins();
DRING_PUBLIC int installPlugin(const std::string& jplPath, bool force);
DRING_PUBLIC int uninstallPlugin(const std::string& pluginRootPath);
DRING_PUBLIC std::vector<std::string> getCallMediaHandlers();
DRING_PUBLIC std::vector<std::string> getChatHandlers();
DRING_PUBLIC void toggleCallMediaHandler(const std::string& mediaHandlerId,
                                         const std::string& callId,
                                         bool toggle);
DRING_PUBLIC void toggleChatHandler(const std::string& chatHandlerId,
                                    const std::string& accountId,
                                    const std::string& peerId,
                                    bool toggle);
DRING_PUBLIC std::map<std::string, std::string> getCallMediaHandlerDetails(
    const std::string& mediaHandlerId);
DRING_PUBLIC std::vector<std::string> getCallMediaHandlerStatus(const std::string& callId);
DRING_PUBLIC std::map<std::string, std::string> getChatHandlerDetails(
    const std::string& chatHandlerId);
DRING_PUBLIC std::vector<std::string> getChatHandlerStatus(const std::string& accountId,
                                                           const std::string& peerId);
DRING_PUBLIC bool getPluginsEnabled();
DRING_PUBLIC void setPluginsEnabled(bool state);
} // namespace DRing
