/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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

#include "preferencehandler.h"

#include "noncopyable.h"

#include <list>
#include <map>
#include <tuple>
#include <memory>
#include <vector>

namespace jami {

class PluginManager;

using PreferenceHandlerPtr = std::unique_ptr<PreferenceHandler>;

/**
 * @brief This class provides the interface between PreferenceHandlers
 * and per account preferences. Besides, it stores pointers to all loaded PreferenceHandlers;
 */
class PreferenceServicesManager
{
public:
    /**
     * @brief Constructor registers PreferenceHandler API services to the PluginManager
     * instance. These services will store PreferenceHandler pointers or clean them
     * from the Plugin System once a plugin is loaded or unloaded.
     * @param pluginManager
     */
    PreferenceServicesManager(PluginManager& pluginManager);

    ~PreferenceServicesManager();

    NON_COPYABLE(PreferenceServicesManager);

    /**
     * @brief List all PreferenceHandlers available.
     * @return Vector with stored PreferenceHandlers pointers.
     */
    std::vector<std::string> getHandlers() const;

    /**
     * @brief Returns details Map from s implementation.
     * @param preferenceHandlerIdStr
     * @return Details map from the PreferenceHandler implementation
     */
    std::map<std::string, std::string> getHandlerDetails(
        const std::string& preferenceHandlerIdStr) const;

    /**
     * @brief Sets a preference.
     * @param key
     * @param value
     * @param rootPath
     * @param accountId
     * @return False if preference was changed.
     */
    bool setPreference(const std::string& key,
                       const std::string& value,
                       const std::string& rootPath,
                       const std::string& accountId);

    /**
     * @brief Resets acc preferences to default values.
     * @param rootPath
     * @param accountId
     */
    void resetPreferences(const std::string& rootPath, const std::string& accountId);

private:
    /**
     * @brief Exposes PreferenceHandlers' life cycle managers services to the main API.
     * @param pluginManager
     */
    void registerComponentsLifeCycleManagers(PluginManager& pluginManager);

    // Components that a plugin can register through registerPreferenceHandler service.
    std::list<PreferenceHandlerPtr> handlers_;
};
} // namespace jami
