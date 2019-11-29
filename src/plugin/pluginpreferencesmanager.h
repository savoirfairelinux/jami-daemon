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
     * @brief savePluginPreferenceValue
     * @param plugin rootPath
     * @param key
     * @param value
     * @return true is success
     */
    bool savePluginPreferenceValue(const std::string& rootPath, const std::string& key, const std::string& value) {
        return PluginPreferencesValuesManager::savePreferenceValue(pluginPreferencesValuesFilePath(rootPath), key, value);
    }
    
    // TODO : improve getPluginDetails
    /**
     * @brief getPluginDetails
     * Returns the tuple (name, description, icon path, so path)
     * The icon should ideally be 192x192 pixels or better 512x512 pixels
     * In order to match with android specifications
     * https://developer.android.com/google-play/resources/icon-design-specifications
     * @param plugin rootPath
     * @return map where the keyset is {"name", "description", "iconPath"}
     */
    std::map<std::string, std::string> getPluginDetails(const std::string& rootPath) {
        std::map<std::string, std::string> details;
        std::string pluginName = rootPath.substr(rootPath.find_last_of("/\\") + 1);
        details["name"] = pluginName;
        details["description"] = "A simple description";
        details["iconPath"] = rootPath + DIR_SEPARATOR_CH + "data" + DIR_SEPARATOR_CH + "icon.png";
        details["soPath"] = rootPath + DIR_SEPARATOR_CH + "lib" + pluginName + ".so";
        return details;
    }
    
    /**
     * @brief listPlugins
     * @return 
     */
    std::vector<std::string> listPlugins() {
        std::string pluginsPath = fileutils::get_data_dir() + DIR_SEPARATOR_CH + "plugins";
        std::vector<std::string> pluginsPaths = fileutils::readDirectory(pluginsPath);
        std::for_each(pluginsPaths.begin(), pluginsPaths.end(),
                      [&pluginsPath](std::string& x){ x = pluginsPath + DIR_SEPARATOR_CH + x;});
        return pluginsPaths;
    }
    
    /**
     * @brief addPlugin
     * @param jplPath
     * @return 
     */
    int addPlugin(const std::string& jplPath) {
        int i{0};
        if(fileutils::isFile(jplPath)) {
            std::string pluginName = jplPath.substr(jplPath.find_last_of("/\\") + 1);
            pluginName = pluginName.substr(0, pluginName.find_last_of('.'));
            i = static_cast<int>(archiver::uncompressArchive(jplPath, fileutils::get_data_dir() + DIR_SEPARATOR_CH + "plugins"
                                                                          + DIR_SEPARATOR_CH + pluginName));
        }
        return i;
    }
    
    /**
     * @brief removePlugin
     * Removes plugin folder
     * @param pluginRootPath
     * @return 
     */
    int removePlugin(const std::string& pluginRootPath){
        if(checkPluginValidity(pluginRootPath)) {
          return fileutils::removeAll(pluginRootPath);
        } else {
            return -1;
        }
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
    
    /**
     * @brief checkPluginValidity
     * @return 
     */
    bool checkPluginValidity(const std::string& pluginRootPath) {
        (void) pluginRootPath;
        return true;
    }
};
}

