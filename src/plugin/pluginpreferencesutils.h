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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA.
 */

#pragma once

#include <json/json.h>
#include <string>
#include <set>
#include <filesystem>

namespace jami {

using ChatHandlerList = std::map<std::pair<std::string, std::string>, std::map<std::string, bool>>;

/**
 * @class  PluginPreferencesUtils
 * @brief Static class that gathers functions to manage
 * plugins' preferences.
 */
class PluginPreferencesUtils
{
public:
    /**
     * @brief Given a plugin installation path, returns the path to the
     * preference.json of this plugin.
     * @param rootPath
     * @param accountId
     * @return preference.json file path.
     */
    static std::filesystem::path getPreferencesConfigFilePath(const std::filesystem::path& rootPath,
                                                    const std::string& accountId = "");

    /**
     * @brief Given a plugin installation path, returns the path to the
     * preference.msgpack file.
     * The preference.msgpack file saves the actuall preferences values
     * if they were modified.
     * @param rootPath
     * @param accountId
     * @return preference.msgpack file path.
     */
    static std::filesystem::path valuesFilePath(const std::filesystem::path& rootPath,
                                      const std::string& accountId = "");

    /**
     * @brief Returns the path to allowdeny.msgpack file.
     * The allowdeny.msgpack file persists ChatHandlers status for each
     * conversation this handler was previously (de)activated.
     * @return allowdeny.msgpack file path.
     */
    static std::filesystem::path getAllowDenyListsPath();

    /**
     * @brief Returns a colon separated string with values from a json::Value containing an array.
     * @param jsonArray
     * @return Colon separated string with jsonArray contents.
     */
    static std::string convertArrayToString(const Json::Value& jsonArray);

    /**
     * @brief Parses a single preference from json::Value to a Map<string, string>.
     * @param jsonPreference
     * @return std::map<std::string, std::string> preference
     */
    static std::map<std::string, std::string> parsePreferenceConfig(
        const Json::Value& jsonPreference);

    /**
     * @brief Reads a preference.json file from the plugin installed in rootPath.
     * @param rootPath
     * @param accountId
     * @return std::vector<std::map<std::string, std::string>> with preferences.json content
     */
    static std::vector<std::map<std::string, std::string>> getPreferences(
        const std::filesystem::path& rootPath, const std::string& accountId = "");

    /**
     * @brief Reads preferences values which were modified from defaultValue
     * @param rootPath
     * @param accountId
     * @return Map with preference keys and actuall values.
     */
    static std::map<std::string, std::string> getUserPreferencesValuesMap(
        const std::filesystem::path& rootPath, const std::string& accountId = "");

    /**
     * @brief Reads preferences values
     * @param rootPath
     * @param accountId
     * @return Map with preference keys and actuall values.
     */
    static std::map<std::string, std::string> getPreferencesValuesMap(
        const std::filesystem::path& rootPath, const std::string& accountId = "");

    /**
     * @brief Resets all preferences values to their defaultValues
     * by erasing all data saved in preferences.msgpack.
     * @param rootPath
     * @param accountId
     * @return True if preferences were reset.
     */
    static bool resetPreferencesValuesMap(const std::string& rootPath, const std::string& accountId);

    /**
     * @brief Saves ChantHandlers status provided by list.
     * @param [in] list
     */
    static void setAllowDenyListPreferences(const ChatHandlerList& list);

    /**
     * @brief Reads ChantHandlers status from allowdeny.msgpack file.
     * @param [out] list
     */
    static void getAllowDenyListPreferences(ChatHandlerList& list);

    /**
     * @brief Creates a "always" preference for a handler if this preference doesn't exist yet.
     * A "always" preference tells the Plugin System if in the event of a new call or chat message,
     * the handler is suposed to be automatically activated.
     * @param handlerName
     * @param rootPath
     */
    static void addAlwaysHandlerPreference(const std::string& handlerName,
                                           const std::string& rootPath);

    /**
     * @brief Read plugin's preferences and returns wheter a specific handler
     * "always" preference is True or False.
     * @param rootPath
     * @param handlerName
     * @param accountId
     * @return True if the handler should be automatically toggled
     */
    static bool getAlwaysPreference(const std::string& rootPath,
                                    const std::string& handlerName,
                                    const std::string& accountId);

private:
    PluginPreferencesUtils() {}
    ~PluginPreferencesUtils() {}
};
} // namespace jami
