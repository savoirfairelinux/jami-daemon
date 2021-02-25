/*
 *  Copyright (C) 2020-2021 Savoir-faire Linux Inc.
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
#include "pluginmanager.h"
#include "pluginpreferencesutils.h"

#include "callservicesmanager.h"
#include "chatservicesmanager.h"

#include <vector>
#include <map>
#include <list>
#include <algorithm>

namespace jami {

/**
 * @class  JamiPluginManager
 * @brief This class provides an interface to functions exposed to the
 * Plugin System interface for lrc and clients.
 */
class JamiPluginManager
{
public:
    JamiPluginManager()
        : callsm_ {pm_}
        , chatsm_ {pm_}
    {
        registerServices();
    }

    /**
     * @brief Parses a manifest file and return its content
     * along with other internally added values.
     * @param rootPath installation path
     * @return Map where the keyset is {"name", "description", "version", "iconPath", "soPath"}
     */
    std::map<std::string, std::string> getPluginDetails(const std::string& rootPath);

    /**
     * @brief Returns a vector with installed plugins
     */
    std::vector<std::string> getInstalledPlugins();

    /**
     * @brief Checks if the plugin has a valid manifest, installs the plugin if not
     * previously installed or if installing a newer version of it.
     * @param jplPath
     * @param force If rue, allows installing an older plugin version.
     * @return 0 if success
     * 100 if already installed with similar version
     * 200 if already installed with newer version
     * 300 if signature is not verified
     * libarchive error codes otherwise
     */
    int installPlugin(const std::string& jplPath, bool force);

    /**
     * @brief Checks if the plugin has a valid manifest, if plugin is loaded,
     * tries to unload it and then removes plugin folder.
     * @param rootPath
     * @return 0 if success
     */
    int uninstallPlugin(const std::string& rootPath);

    /**
     * @brief Returns True if success
     * @param rootPath of the plugin folder
     */
    bool loadPlugin(const std::string& rootPath);

    /**
     * @brief Returns True if success
     * @param rootPath of the plugin folder
     */
    bool unloadPlugin(const std::string& rootPath);

    /**
     * @brief Returns vector with rootpaths of the loaded plugins
     */
    std::vector<std::string> getLoadedPlugins() const;

    /**
     * @brief Returns contents of plugin's preferences.json file
     * @param rootPath
     */
    std::vector<std::map<std::string, std::string>> getPluginPreferences(const std::string& rootPath);

    /**
     * @brief Returns a Map with preferences keys and values.
     * @param rootPath
     */
    std::map<std::string, std::string> getPluginPreferencesValuesMap(const std::string& rootPath);

    /**
     * @brief Modifies a preference value by saving it to a preferences.msgpack.
     * Plugin is reloaded only if the preference cannot take effect immediately.
     * In other words, if we have to reload plugin so that preference may take effect.
     * @param rootPath
     * @param key
     * @param value
     * @return True if success
     */
    bool setPluginPreference(const std::string& rootPath,
                             const std::string& key,
                             const std::string& value);

    /**
     * @brief Reset plugin's preferences values to their defaultValues
     * @param rootPath
     * @return True if success.
     */
    bool resetPluginPreferencesValuesMap(const std::string& rootPath);

    /** To be used during installation
     * @brief Checks plugin binary and info files signatures with public key provided inside jpl
     * if any of the checks fail, plugin cannot be installed
     * @param rootPath
     * @param name
     * @return True if plugin binary signature check succeed
     */
    bool checkSignature(const std::string& rootPath, const std::string& name);

    /** To be used during installation
     * If both files are checked, we provide plugin authorship infos to the user
     * waiting it's approval. If aproved, proceed installation. Exit installation
     * process otherwise.
     * @brief Verify if user accepts author infos before loading plugin
     * @param rootPath
     * @param name
     * @return true if authors are accepted
     */
    bool checkPluginAuthors(const std::string& rootPath, const std::string& name);

    CallServicesManager& getCallServicesManager() { return callsm_; }

    ChatServicesManager& getChatServicesManager() { return chatsm_; }

private:
    NON_COPYABLE(JamiPluginManager);

    /**
     * @brief Register services that can be called from plugin implementation side.
     */
    void registerServices();

    // PluginManager instance
    PluginManager pm_;

    // Map between plugins installation path and manifest infos.
    std::map<std::string, std::map<std::string, std::string>> pluginDetailsMap_;

    // Services instances
    CallServicesManager callsm_;
    ChatServicesManager chatsm_;
};
} // namespace jami
