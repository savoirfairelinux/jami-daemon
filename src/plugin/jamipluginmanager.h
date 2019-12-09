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

#include <vector>
#include <map>
#include <string>
#include <algorithm>

namespace jami {
class JamiPluginManager
{
public:
    JamiPluginManager() = default;
    NON_COPYABLE(JamiPluginManager);
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
        std::map<std::string, std::string> details = parseManifestFile(manifestPath(rootPath));
        details["iconPath"] = rootPath + DIR_SEPARATOR_CH + "data" + DIR_SEPARATOR_CH + "icon.png";
        details["soPath"] = rootPath + DIR_SEPARATOR_CH + "lib" + details["name"] + ".so";
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
        auto predicate = [this](std::string path){ return !checkPluginValidity(path);};
        auto returnIterator = std::remove_if(pluginsPaths.begin(),pluginsPaths.end(),predicate);
        pluginsPaths.erase(returnIterator,std::end(pluginsPaths));
        return pluginsPaths;
    }

    /**
     * @brief installPlugin
     * Checks if the plugin has a valid manifest, installs the plugin if not previously installed
     * or if installing a newer version of it
     * If force is true, we force install the plugin
     * @param jplPath
     * @param force
     * @return 
     */
    int installPlugin(const std::string& jplPath, bool force);

    /**
     * @brief uninstallPlugin
     * Checks if the plugin has a valid manifest then removes plugin folder
     * @param pluginRootPath
     * @return 
     */
    int uninstallPlugin(const std::string& pluginRootPath){
        if(checkPluginValidity(pluginRootPath)) {
            return fileutils::removeAll(pluginRootPath);
        } else {
            return -1;
        }
    }
    
private:
    
    /**
     * @brief checkPluginValidity
     * Checks if the plugin has a manifest file with a name and a version
     * @return 0 if valid, -50l if the manifest doesn't conform to the plugin standard
     * -40l if failed to parse the manifest file
     */
    bool checkPluginValidity(const std::string& pluginRootPath) {       
        return !parseManifestFile(manifestPath(pluginRootPath)).empty();
    }
    
    /**
     * @brief readPluginManifestFromArchive
     * Reads the manifest file content without uncompressing the whole archive
     * Maps the manifest data to a map(string, string)
     * @param jplPath
     * @param details
     * @return 0 if valid, -50l if the manifest doesn't conform to the plugin standard
     * -40l if failed to parse the manifest file
     */
    long readPluginManifestFromArchive(const std::string& jplPath, std::map<std::string, std::string>& details);
    
    std::map<std::string, std::string> parseManifestFile(const std::string &manifestFilePath);
    
    const std::string manifestPath(const std::string& pluginRootPath) {
        return pluginRootPath + DIR_SEPARATOR_CH + "manifest.json";
    }
};
}


