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
#include "pluginpreferencesparser.h"

#include <vector>
#include <map>
#include <string>

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
     * @brief getSavedPreferencesValuesMap
     * @param pluginSoPath
     * @return 
     */
    std::map<std::string, std::string> getPluginPreferencesValuesMap(const std::string& pluginSoPath);
    
    /**
     * @brief savePluginPreferenceValue
     * @param pluginSoPath
     * @param key
     * @param value
     * @return true is success
     */
    bool savePluginPreferenceValue(const std::string& pluginSoPath, const std::string& key, const std::string& value);
    
    /**
     * @brief getPluginIconPath
     * Returns the plugin Icon absolute path
     * This is entirely defined by how the plugin files are structured
     * The icon should ideally be 192x192 pixels or better 512x512 pixels
     * In order to match with android specifications
     * https://developer.android.com/google-play/resources/icon-design-specifications
     * @param pluginSoPath
     * @return absolute path to the icon.png file
     */
    std::string getPluginIconPath(const std::string& pluginSoPath) {
        std::string rootPath = pluginSoPath.substr(0, pluginSoPath.rfind(this->separator()));
        return rootPath + this->separator() + "data" + this->separator() + "icon.png";
    }
    
private:
    
    inline char separator() const
    {
        #ifdef _WIN32
                return '\\';
        #else
                return '/';
        #endif
    }
    
    /**
     * @brief getPreferencesConfigFilePath
     * Returns the plugin preferences config file path from the plugin so file path
     * This is entirely defined by how the plugin files are structured
     * @param pluginSoPath
     * @return path of the preferences config
     */
    std::string getPreferencesConfigFilePath(const std::string& pluginSoPath) const {
        std::string rootPath = pluginSoPath.substr(0, pluginSoPath.rfind(this->separator()));
        return rootPath + this->separator() + "data" + this->separator() + "preferences.json";
    }
    
    /**
     * @brief pluginPreferencesValuesFilePath
     * Returns the plugin preferences values file path from the plugin so file path
     * This is entirely defined by how the plugin files are structured
     * @param pluginSoPath
     * @return path of the preferences values
     */
    std::string pluginPreferencesValuesFilePath(const std::string& pluginSoPath) const {
        std::string rootPath = pluginSoPath.substr(0, pluginSoPath.rfind(this->separator()));
        return rootPath + this->separator() + "data" + this->separator() + "preferences.msgpack";
    }
};
}

