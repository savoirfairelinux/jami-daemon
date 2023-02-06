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

#pragma once

#include "noncopyable.h"
#include "plugin/webviewservicesmanager.h"
#include "pluginmanager.h"
#include "pluginpreferencesutils.h"

#include "callservicesmanager.h"
#include "chatservicesmanager.h"
#include "preferenceservicesmanager.h"

#include <vector>
#include <map>
#include <list>
#include <algorithm>

namespace jami {

using PreferencesMap = std::map<std::string, std::map<std::string, std::string>>;

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
        , webviewsm_ {pm_}
        , preferencesm_ {pm_}
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
     * @param force If true, allows installing an older plugin version.
     * @return 0 if success
     * 100 if already installed with similar version
     * 200 if already installed with newer version
     * libarchive (mizip in apple platforms) error codes otherwise
     */
    int installPlugin(const std::string& jplPath, bool force);

    /**
     * @brief Checks if the plugin has a valid manifest and if the plugin is loaded,
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
     * @param accountId
     */
    std::vector<std::map<std::string, std::string>> getPluginPreferences(
        const std::string& rootPath, const std::string& accountId);

    /**
     * @brief Returns a Map with preferences keys and values.
     * @param rootPath
     * @param accountId
     */
    std::map<std::string, std::string> getPluginPreferencesValuesMap(const std::string& rootPath,
                                                                     const std::string& accountId);

    /**
     * @brief Modifies a preference value by saving it to a preferences.msgpack.
     * Plugin is reloaded only if the preference cannot take effect immediately.
     * In other words, if we have to reload plugin so that preference may take effect.
     * @param rootPath
     * @param accountId
     * @param key
     * @param value
     * @return True if success
     */
    bool setPluginPreference(const std::string& rootPath,
                             const std::string& accountId,
                             const std::string& key,
                             const std::string& value);

    /**
     * @brief Reset plugin's preferences values to their defaultValues
     * @param rootPath
     * @param accountId
     * @return True if success.
     */
    bool resetPluginPreferencesValuesMap(const std::string& rootPath, const std::string& accountId);

    CallServicesManager& getCallServicesManager() { return callsm_; }

    ChatServicesManager& getChatServicesManager() { return chatsm_; }

    WebViewServicesManager& getWebViewServicesManager() { return webviewsm_; }

    PreferenceServicesManager& getPreferenceServicesManager() { return preferencesm_; }

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
    WebViewServicesManager webviewsm_;
    PreferenceServicesManager preferencesm_;
};
} // namespace jami
