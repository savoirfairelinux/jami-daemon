/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
#include "fileutils.h"
#include "archiver.h"
#include "pluginpreferencesparser.h"
#include "pluginpreferencesvaluesmanager.h"

#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <algorithm> 

namespace jami {
class PluginPreferencesManager
{
public:
    PluginPreferencesManager() = default;
    NON_COPYABLE(PluginPreferencesManager);

    /**
     * @brief getPluginPreferences
     * Parses the plugin preferences configuration file
     * @param path
     * @return 
     */
    std::vector<MapStrStr> getPluginPreferences(const std::string& path) {
        return PluginPreferencesParser::parsePreferencesConfigFile(getPreferencesConfigFilePath(path));
    }

    /**
     * @brief gets Plugin saved preferences values Map
     * @param plugin rootPath
     * @return 
     */
    std::map<std::string, std::string> getPluginPreferencesValuesMap(const std::string& rootPath) {
        return PluginPreferencesValuesManager::getPreferencesValuesMap(pluginPreferencesValuesFilePath(rootPath));
    }

    /**
     * @brief resetPluginPreferencesValuesMap
     * @param rootPath
     */
    bool resetPluginPreferencesValuesMap(const std::string& rootPath) {
        return PluginPreferencesValuesManager::resetPluginPreferencesValuesMap(pluginPreferencesValuesFilePath(rootPath));
    }

    /**
     * @brief savePluginPreferenceValue
     * @param plugin rootPath
     * @param key
     * @param value
     * @return true is success
     */
    bool savePluginPreferenceValue(const std::string& rootPath, const std::string& key, const std::string& value) {
        return PluginPreferencesValuesManager::savePreferenceValue(pluginPreferencesValuesFilePath(rootPath), key, value);
    }

private:
    /**
     * @brief getPreferencesConfigFilePath
     * Returns the plugin preferences config file path from the plugin so file path
     * This is entirely defined by how the plugin files are structured
     * @param plugin rootPath
     * @return path of the preferences config
     */
    std::string getPreferencesConfigFilePath(const std::string& rootPath) const {
        return rootPath + DIR_SEPARATOR_CH + "data" + DIR_SEPARATOR_CH + "preferences.json";
    }

    /**
     * @brief pluginPreferencesValuesFilePath
     * Returns the plugin preferences values file path from the plugin so file path
     * This is entirely defined by how the plugin files are structured
     * @param plugin rootPath
     * @return path of the preferences values
     */
    std::string pluginPreferencesValuesFilePath(const std::string& rootPath) const {
        return rootPath + DIR_SEPARATOR_CH + "data" + DIR_SEPARATOR_CH + "preferences.msgpack";
    }
};
}

