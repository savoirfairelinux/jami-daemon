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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA.
 */

#include "pluginpreferencesutils.h"

#include <msgpack.hpp>
#include <sstream>
#include <fstream>
#include "logger.h"
#include "fileutils.h"

namespace jami {

/*!
 * \brief Given a plugin installation path, returns the path to the
 * preference.json of this plugin.
 * \param rootPath
 * \return preference.json file path.
 */
std::string
PluginPreferencesUtils::getPreferencesConfigFilePath(const std::string& rootPath)
{
    return rootPath + DIR_SEPARATOR_CH + "data" + DIR_SEPARATOR_CH + "preferences.json";
}

/*!
 * \brief Given a plugin installation path, returns the path to the
 * preference.msgpack file.
 * The preference.msgpack file saves the actuall preferences values
 * if they were modified.
 * \param rootPath
 * \return preference.msgpack file path.
 */
std::string
PluginPreferencesUtils::valuesFilePath(const std::string& rootPath)
{
    return rootPath + DIR_SEPARATOR_CH + "preferences.msgpack";
}

/*!
 * \brief Returns the path to allowdeny.msgpack file.
 * The allowdeny.msgpack file persists ChatHandlers status for each
 * conversation this handler was previously (de)activated.
 * \return allowdeny.msgpack file path.
 */
std::string
PluginPreferencesUtils::getAllowDenyListsPath()
{
    return fileutils::get_data_dir() + DIR_SEPARATOR_CH + "plugins" + DIR_SEPARATOR_CH
           + "allowdeny.msgpack";
}

/*!
 * \brief Returns a colon separated string with values from a json::Value containing an array.
 * \param jsonArray
 * \return Colon separated string with jsonArray contents.
 */
std::string
PluginPreferencesUtils::convertArrayToString(const Json::Value& jsonArray)
{
    std::string stringArray {};

    if (jsonArray.size()) {
        for (unsigned i = 0; i < jsonArray.size() - 1; i++) {
            if (jsonArray[i].isString()) {
                stringArray += jsonArray[i].asString() + ",";
            } else if (jsonArray[i].isArray()) {
                stringArray += convertArrayToString(jsonArray[i]) + ",";
            }
        }

        unsigned lastIndex = jsonArray.size() - 1;
        if (jsonArray[lastIndex].isString()) {
            stringArray += jsonArray[lastIndex].asString();
        }
    }

    return stringArray;
}

/*!
 * \brief Parses a single preference from json::Value to a Map<string, string>.
 * \param jsonPreference
 * \return std::map<std::string, std::string> preference
 */
std::map<std::string, std::string>
PluginPreferencesUtils::parsePreferenceConfig(const Json::Value& jsonPreference)
{
    std::map<std::string, std::string> preferenceMap;
    const auto& members = jsonPreference.getMemberNames();
    /// Insert other fields
    for (const auto& member : members) {
        const Json::Value& value = jsonPreference[member];
        if (value.isString()) {
            preferenceMap.emplace(member, jsonPreference[member].asString());
        } else if (value.isArray()) {
            preferenceMap.emplace(member, convertArrayToString(jsonPreference[member]));
        }
    }
    return preferenceMap;
}

/*!
 * \brief Reads a preference.json file from the plugin installed in rootPath.
 * \param rootPath
 * \return std::vector<std::map<std::string, std::string>> with preferences.json content
 */
std::vector<std::map<std::string, std::string>>
PluginPreferencesUtils::getPreferences(const std::string& rootPath)
{
    std::string preferenceFilePath = getPreferencesConfigFilePath(rootPath);
    std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferenceFilePath));
    std::ifstream file(preferenceFilePath);
    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    std::set<std::string> keys;
    std::vector<std::map<std::string, std::string>> preferences;
    if (file) {
        /// reads the file to a json format
        bool ok = Json::parseFromStream(rbuilder, file, &root, &errs);
        if (ok && root.isArray()) {
            /// read each preference described in preference.json individually
            for (unsigned i = 0; i < root.size(); i++) {
                const Json::Value& jsonPreference = root[i];
                std::string category = jsonPreference.get("category", "NoCategory").asString();
                std::string type = jsonPreference.get("type", "None").asString();
                std::string key = jsonPreference.get("key", "None").asString();
                /// the preference must have at leat type and key
                if (type != "None" && key != "None") {
                    if (keys.find(key) == keys.end()) {
                        /// read the rest of the preference
                        auto preferenceAttributes = parsePreferenceConfig(jsonPreference);
                        /// If the parsing of the attributes was successful, commit the map and the keys
                        auto defaultValue = preferenceAttributes.find("defaultValue");
                        if (type == "Path" && defaultValue != preferenceAttributes.end()) {
                            /// defaultValue in a Path preference is an incomplete path
                            /// starting from the installation path of the plugin.
                            /// Here we complete the path value.
                            defaultValue->second = rootPath + DIR_SEPARATOR_STR
                                                   + defaultValue->second;
                        }

                        if (!preferenceAttributes.empty()) {
                            preferences.push_back(std::move(preferenceAttributes));
                            keys.insert(key);
                        }
                    }
                }
            }
        } else {
            JAMI_ERR() << "PluginPreferencesParser:: Failed to parse preferences.json for plugin: "
                       << preferenceFilePath;
        }
    }

    return preferences;
}

/*!
 * \brief Reads preferences values which were modified from defaultValue
 * \param rootPath
 * \return Map with preference keys and actuall values.
 */
std::map<std::string, std::string>
PluginPreferencesUtils::getUserPreferencesValuesMap(const std::string& rootPath)
{
    const std::string preferencesValuesFilePath = valuesFilePath(rootPath);
    std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferencesValuesFilePath));
    std::ifstream file(preferencesValuesFilePath, std::ios::binary);
    std::map<std::string, std::string> rmap;

    /// If file is accessible
    if (file.good()) {
        /// Get file size
        std::string str;
        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        /// If not empty
        if (fileSize > 0) {
            /// Read whole file content and put it in the string str
            str.reserve(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            str.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            try {
                /// Unpack the string
                msgpack::object_handle oh = msgpack::unpack(str.data(), str.size());
                /// Deserialized object is valid during the msgpack::object_handle instance is alive.
                msgpack::object deserialized = oh.get();
                deserialized.convert(rmap);
            } catch (const std::exception& e) {
                JAMI_ERR() << e.what();
            }
        }
    }
    return rmap;
}

/*!
 * \brief Reads preferences values
 * \param rootPath
 * \return Map with preference keys and actuall values.
 */
std::map<std::string, std::string>
PluginPreferencesUtils::getPreferencesValuesMap(const std::string& rootPath)
{
    std::map<std::string, std::string> rmap;

    /// reads all preferences values
    std::vector<std::map<std::string, std::string>> preferences = getPreferences(rootPath);
    for (auto& preference : preferences) {
        rmap[preference["key"]] = preference["defaultValue"];
    }

    /// if any of these preferences were modified, it's value is changed before return
    for (const auto& pair : getUserPreferencesValuesMap(rootPath)) {
        rmap[pair.first] = pair.second;
    }

    return rmap;
}

/*!
 * \brief Resets all preferences values to their defaultValues
 * by erasing all data saved in preferences.msgpack.
 * \param rootPath
 * \return True if preferences were reset.
 */
bool
PluginPreferencesUtils::resetPreferencesValuesMap(const std::string& rootPath)
{
    bool returnValue = true;
    std::map<std::string, std::string> pluginPreferencesMap {};

    const std::string preferencesValuesFilePath = valuesFilePath(rootPath);
    std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferencesValuesFilePath));
    std::ofstream fs(preferencesValuesFilePath, std::ios::binary);
    if (!fs.good()) {
        return false;
    }
    try {
        msgpack::pack(fs, pluginPreferencesMap);
    } catch (const std::exception& e) {
        returnValue = false;
        JAMI_ERR() << e.what();
    }

    return returnValue;
}

/*!
 * \brief Saves ChantHandlers status provided by list.
 * \param [in] list
 */
void
PluginPreferencesUtils::setAllowDenyListPreferences(const ChatHandlerList& list)
{
    std::string filePath = getAllowDenyListsPath();
    std::lock_guard<std::mutex> guard(fileutils::getFileLock(filePath));
    std::ofstream fs(filePath, std::ios::binary);
    if (!fs.good()) {
        return;
    }
    try {
        msgpack::pack(fs, list);
    } catch (const std::exception& e) {
        JAMI_ERR() << e.what();
    }
}

/*!
 * \brief Reads ChantHandlers status from allowdeny.msgpack file.
 * \param [out] list
 */
void
PluginPreferencesUtils::getAllowDenyListPreferences(ChatHandlerList& list)
{
    const std::string filePath = getAllowDenyListsPath();
    std::lock_guard<std::mutex> guard(fileutils::getFileLock(filePath));
    std::ifstream file(filePath, std::ios::binary);

    /// If file is accessible
    if (file.good()) {
        /// Get file size
        std::string str;
        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        /// If not empty
        if (fileSize > 0) {
            /// Read whole file content and put it in the string str
            str.reserve(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            str.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            try {
                /// Unpack the string
                msgpack::object_handle oh = msgpack::unpack(str.data(), str.size());
                /// Deserialized object is valid during the msgpack::object_handle instance is alive.
                msgpack::object deserialized = oh.get();
                deserialized.convert(list);
            } catch (const std::exception& e) {
                JAMI_ERR() << e.what();
            }
        }
    }
}

/*!
 * \brief Creates a "always" preference for a handler if this preference doesn't exist yet.
 * A "always" preference tells the Plugin System if in the event of a new call or chat message,
 * the handler is suposed to be automaticaly activated.
 * \param handlerName
 * \param rootPath
 */
void
PluginPreferencesUtils::addAlwaysHandlerPreference(const std::string& handlerName,
                                                   const std::string& rootPath)
{
    std::string filePath = getPreferencesConfigFilePath(rootPath);
    Json::Value root;
    {
        std::lock_guard<std::mutex> guard(fileutils::getFileLock(filePath));
        std::ifstream file(filePath);
        Json::CharReaderBuilder rbuilder;
        Json::Value preference;
        rbuilder["collectComments"] = false;
        std::string errs;
        std::set<std::string> keys;
        std::vector<std::map<std::string, std::string>> preferences;
        if (file) {
            bool ok = Json::parseFromStream(rbuilder, file, &root, &errs);
            if (ok && root.isArray()) {
                /// return if preference already exists
                for (const auto& child : root)
                    if (child.get("key", "None").asString() == handlerName + "Always")
                        return;
            }
            /// create preference structure otherwise
            preference["key"] = handlerName + "Always";
            preference["type"] = "Switch";
            preference["defaultValue"] = "0";
            preference["title"] = "Automatically turn " + handlerName + " on";
            preference["summary"] = handlerName + " will take effect immediatly";
            root.append(preference);
            file.close();
        }
    }
    {
        std::lock_guard<std::mutex> guard(fileutils::getFileLock(filePath));
        std::ofstream outFile(filePath);
        if (outFile) {
            /// save preference.json file with new "always preference"
            outFile << root.toStyledString();
            outFile.close();
        }
    }
}

/*!
 * \brief Read plugin's preferences and returns wheter a specific handler
 * "always" preference is True or False.
 * \param rootPath
 * \param handlerName
 * \return True if the handler should be automatically toggled
 */
bool
PluginPreferencesUtils::getAlwaysPreference(const std::string& rootPath, std::string& handlerName)
{
    std::vector<std::map<std::string, std::string>> preferences = getPreferences(rootPath);
    std::map<std::string, std::string> preferencesValues = getPreferencesValuesMap(rootPath);

    for (auto preference : preferences) {
        auto key = preference.at("key");
        if (preference.at("type") == "Switch" && key == handlerName + "Always"
            && preferencesValues.find(key)->second == "1")
            return true;
    }

    return false;
}
} // namespace jami
