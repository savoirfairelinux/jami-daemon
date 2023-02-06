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

#include <string>
#include <map>

namespace jami {

/**
 * @brief This abstract class is an API we need to implement from plugin side.
 * In other words, a plugin functionality that handles preferences per account
 * must start from the implementation of this class.
 */
class PreferenceHandler
{
public:
    virtual ~PreferenceHandler() {}

    /**
     * @brief Returns a map with handler's name, iconPath, and pluginId.
     */
    virtual std::map<std::string, std::string> getHandlerDetails() = 0;

    /**
     * @brief If a preference can have different values depending on accountId, those values should
     * be stored in the plugin through this function.
     * @param accountId
     * @param key
     * @param value
     */
    virtual void setPreferenceAttribute(const std::string& accountId,
                                        const std::string& key,
                                        const std::string& value)
        = 0;

    /**
     * @brief If a preference can be stored as per accountId, this function should return True.
     * @param key
     * @return True if preference can be changed through setPreferenceAttribute method.
     */
    virtual bool preferenceMapHasKey(const std::string& key) = 0;

    /**
     * @brief Reset stored preferences for given accountId.
     * @param accountId
     */
    virtual void resetPreferenceAttributes(const std::string& accountId) = 0;

    /**
     * @brief Returns the dataPath of the plugin that created this PreferenceHandler.
     */
    std::string id() const { return id_; }

    /**
     * @brief Should be called by the PreferenceHandler creator to set the plugins id_ variable.
     */
    virtual void setId(const std::string& id) final { id_ = id; }

private:
    // Is the dataPath of the plugin that created this ChatHandler.
    std::string id_;
};
} // namespace jami
