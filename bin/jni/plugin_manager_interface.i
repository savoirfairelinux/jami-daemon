/*
 *  Copyright (C) 2020 Savoir-faire Linux Inc.
 *
 *  Authors: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

%header %{

#include "dring/dring.h"
#include "dring/plugin_manager_interface.h"
%}

namespace DRing {
bool loadPlugin(const std::string& path);
bool unloadPlugin(const std::string& path);
void togglePlugin(const std::string& path, bool toggle);
std::map<std::string,std::string> getPluginDetails(const std::string& path);
std::vector<std::map<std::string,std::string>> getPluginPreferences(const std::string& path);
bool setPluginPreference(const std::string& path, const std::string& key, const std::string& value);
std::map<std::string,std::string> getPluginPreferencesValues(const std::string& path);
bool resetPluginPreferencesValues(const std::string& path);
std::vector<std::string> listAvailablePlugins();
std::vector<std::string> listLoadedPlugins();
int installPlugin(const std::string& jplPath, bool force);
int uninstallPlugin(const std::string& pluginRootPath);
std::vector<std::string> listCallMediaHandlers();
void toggleCallMediaHandler(const std::string& mediaHandlerId, const std::string& callId, bool toggle);
std::map<std::string,std::string> getCallMediaHandlerDetails(const std::string& mediaHandlerId);
bool getPluginsEnabled();
void setPluginsEnabled(bool state);
std::map<std::string, std::vector<std::string>> getCallMediaHandlerStatus(const std::string& callId);
}
