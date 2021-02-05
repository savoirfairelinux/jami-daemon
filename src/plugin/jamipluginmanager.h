/*!
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

/*! \class  JamiPluginManager
 * \brief This class provides an interface to functions exposed to the
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

    std::map<std::string, std::string> getPluginDetails(const std::string& rootPath);

    std::vector<std::string> getInstalledPlugins();

    int installPlugin(const std::string& jplPath, bool force);

    int uninstallPlugin(const std::string& rootPath);

    bool loadPlugin(const std::string& rootPath);

    bool unloadPlugin(const std::string& rootPath);

    std::vector<std::string> getLoadedPlugins() const;

    std::vector<std::map<std::string, std::string>> getPluginPreferences(const std::string& rootPath);

    std::map<std::string, std::string> getPluginPreferencesValuesMap(const std::string& rootPath);

    bool setPluginPreference(const std::string& rootPath,
                             const std::string& key,
                             const std::string& value);

    bool resetPluginPreferencesValuesMap(const std::string& rootPath);

    CallServicesManager& getCallServicesManager() { return callsm_; }

    ChatServicesManager& getChatServicesManager() { return chatsm_; }

private:
    NON_COPYABLE(JamiPluginManager);

    void registerServices();

    /// PluginManager instance
    PluginManager pm_;

    /// Map between plugins installation path and manifest infos.
    std::map<std::string, std::map<std::string, std::string>> pluginDetailsMap_;

    /// Services instances
    CallServicesManager callsm_;
    ChatServicesManager chatsm_;
};
} // namespace jami
