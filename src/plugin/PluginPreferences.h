/*
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA.
 */

#pragma once

#include <json/json.h>
#include <string>
#include <set>

namespace jami {

using ChatHandlerList = std::map<std::pair<std::string, std::string>, std::set<std::string>>;


class PluginPreferencesManager {
public:
    static std::string getPreferencesConfigFilePath(const std::string& rootPath);

    static std::string valuesFilePath(const std::string& rootPath);

    static std::string convertArrayToString(const Json::Value& jsonArray);

    static std::map<std::string, std::string> parsePreferenceConfig(const Json::Value& jsonPreference,
                                                            const std::string& type);

    static std::vector<std::map<std::string, std::string>> getPreferences(
        const std::string& rootPath);

    static std::map<std::string, std::string> getUserPreferencesValuesMap(
        const std::string& rootPath);

    static std::map<std::string, std::string> getPreferencesValuesMap(const std::string& rootPath);

    static bool resetPreferencesValuesMap(const std::string& rootPath);

    static std::string getAllowDenyListsPath(bool allow);

    static void setAllowDenyListPreferences(const ChatHandlerList& list, bool allow = true);

    static void getAllowDenyListPreferences(ChatHandlerList& list, bool allow = true);
private:
    PluginPreferencesManager() {}
    ~PluginPreferencesManager() {}
};
} // namespace jami
