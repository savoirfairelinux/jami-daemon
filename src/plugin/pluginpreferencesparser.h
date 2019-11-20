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
// Utils
#include "noncopyable.h"
// JsonCpp
#include <json/json.h>
// Stl
#include <vector>
#include <map>
#include <string>
namespace jami {

using MapStrStr   = std::map<std::string,std::string>;

class PluginPreferencesParser
{
public:
    PluginPreferencesParser() = delete;
    NON_COPYABLE(PluginPreferencesParser);
    
    /**
     * @brief parsePreferencesConfigFile
     * @param path: path to the preferences configuration file
     * @return
     */
    static std::vector<MapStrStr> parsePreferencesConfigFile(const std::string& path);
    
    /**
     * @brief parsePreference
     * Parses preference attributes of the base class Preference
     * @param jsonPreference: A Json::Value object that has key value pairs of attributes
     * @param category: to which category bind this preferenc
     */
    static MapStrStr parsePreferenceConfig(const Json::Value& jsonPreference,
                              const std::string& type, const std::string& category);
    
    /**
     * @brief convertArrayToString
     * Converts an array to a string of the form [elem0, elem1, elem2 ..., elemN-1]
     * works recursively if there are nested arrays
     * @param jsonArray
     * @return string representation of the array
     */
    static std::string convertArrayToString(const Json::Value jsonArray);
};
}

