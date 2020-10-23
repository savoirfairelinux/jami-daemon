/**
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

#pragma once
#include "noncopyable.h"
#include "fileutils.h"
#include "archiver.h"
#include "pluginmanager.h"

// Services
#include "callservicesmanager.h"
#include "conversationservicesmanager.h"

#include <vector>
#include <map>
#include <list>
#include <string>
#include <algorithm>

namespace jami {
class JamiPluginManager
{
public:
    JamiPluginManager()
        : csm_ {pm_}
        , convsm_ {pm_}
    {
        registerServices();
    }
    // TODO : improve getPluginDetails
    /**
     * @brief getPluginDetails
     * Parses a manifest file and returns :
     * The tuple (name, description, version, icon path, so path)
     * The icon should ideally be 192x192 pixels or better 512x512 pixels
     * In order to match with android specifications
     * https://developer.android.com/google-play/resources/icon-design-specifications
     * Saves the result in a map
     * @param plugin rootPath (folder of the plugin)
     * @return map where the keyset is {"name", "description", "iconPath"}
     */
    std::map<std::string, std::string> getPluginDetails(const std::string& rootPath);

    /**
     * @brief listAvailablePlugins
     * Lists available plugins with valid manifest files
     * @return list of plugin directory names
     */
    std::vector<std::string> listAvailablePlugins();

    /**
     * @brief installPlugin
     * Checks if the plugin has a valid manifest, installs the plugin if not previously installed
     * or if installing a newer version of it
     * If force is true, we force install the plugin
     * @param jplPath
     * @param force
     * @return + 0 if success
     * 100 if already installed with similar version
     * 200 if already installed with newer version
     * libarchive error codes otherwise
     */
    int installPlugin(const std::string& jplPath, bool force);

    /**
     * @brief uninstallPlugin
     * Checks if the plugin has a valid manifest then removes plugin folder
     * @param rootPath
     * @return 0 if success
     */
    int uninstallPlugin(const std::string& rootPath);

    /**
     * @brief loadPlugin
     * @param rootPath of the plugin folder
     * @return true is success
     */
    bool loadPlugin(const std::string& rootPath);

    /**
     * @brief unloadPlugin
     * @param rootPath of the plugin folder
     * @return true is success
     */
    bool unloadPlugin(const std::string& rootPath);

    /**
     * @brief togglePlugin
     * @param rootPath of the plugin folder
     * @param toggle: if true, register a new instance of the plugin
     * else, remove the existing instance
     * N.B: before adding a new instance, remove any existing one
     */
    void togglePlugin(const std::string& rootPath, bool toggle);

    /**
     * @brief listLoadedPlugins
     * @return vector of rootpaths of the loaded plugins
     */
    std::vector<std::string> listLoadedPlugins() const;

    std::vector<std::map<std::string, std::string>> getPluginPreferences(const std::string& rootPath);

    bool setPluginPreference(const std::string& rootPath,
                             const std::string& key,
                             const std::string& value);

    std::map<std::string, std::string> getPluginPreferencesValuesMap(const std::string& rootPath);

    bool resetPluginPreferencesValuesMap(const std::string& rootPath);

public:
    CallServicesManager& getCallServicesManager() { return csm_; }

    ConversationServicesManager& getConversationServicesManager() { return convsm_; }

private:
    NON_COPYABLE(JamiPluginManager);

    /**
     * @brief checkPluginValidity
     * Checks if the plugin has a manifest file with a name and a version
     * @return true if valid
     */
    bool checkPluginValidity(const std::string& rootPath)
    {
        return !parseManifestFile(manifestPath(rootPath)).empty();
    }

    /**
     * @brief readPluginManifestFromArchive
     * Reads the manifest file content without uncompressing the whole archive
     * Maps the manifest data to a map(string, string)
     * @param jplPath
     * @return manifest map
     */
    std::map<std::string, std::string> readPluginManifestFromArchive(const std::string& jplPath);

    /**
     * @brief parseManifestFile, parses the manifest file of an installed plugin
     * @param manifestFilePath
     * @return manifest map
     */
    std::map<std::string, std::string> parseManifestFile(const std::string& manifestFilePath);

    std::string manifestPath(const std::string& rootPath)
    {
        return rootPath + DIR_SEPARATOR_CH + "manifest.json";
    }

    std::string getRootPathFromSoPath(const std::string& soPath) const
    {
        return soPath.substr(0, soPath.find_last_of(DIR_SEPARATOR_CH));
    }

    std::string manifestPath(const std::string& rootPath) const
    {
        return rootPath + DIR_SEPARATOR_CH + "manifest.json";
    }

    std::string dataPath(const std::string& pluginSoPath) const
    {
        return getRootPathFromSoPath(pluginSoPath) + DIR_SEPARATOR_CH + "data";
    }

    std::map<std::string, std::string> getPluginUserPreferencesValuesMap(const std::string& rootPath);

    /**
     * @brief getPreferencesConfigFilePath
     * Returns the plugin preferences config file path from the plugin root path
     * This is entirely defined by how the plugin files are structured
     * @param plugin rootPath
     * @return path of the preferences config
     */
    std::string getPreferencesConfigFilePath(const std::string& rootPath) const
    {
        return rootPath + DIR_SEPARATOR_CH + "data" + DIR_SEPARATOR_CH + "preferences.json";
    }

    /**
     * @brief pluginPreferencesValuesFilePath
     * Returns the plugin preferences values file path from the plugin root path
     * This is entirely defined by how the plugin files are structured
     * @param plugin rootPath
     * @return path of the preferences values
     */
    std::string pluginPreferencesValuesFilePath(const std::string& rootPath) const
    {
        return rootPath + DIR_SEPARATOR_CH + "preferences.msgpack";
    }

    void registerServices();

private:
    PluginManager pm_;
    std::map<std::string, std::map<std::string, std::string>> pluginDetailsMap_;

    // Services
private:
    CallServicesManager csm_;
    ConversationServicesManager convsm_;
};
} // namespace jami
