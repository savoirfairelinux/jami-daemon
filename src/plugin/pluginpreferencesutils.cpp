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

#include "pluginpreferencesutils.h"
#include "pluginsutils.h"

#include <msgpack.hpp>
#include <sstream>
#include <fstream>
#include <fmt/core.h>

#include "logger.h"
#include "fileutils.h"

namespace jami {

std::string
PluginPreferencesUtils::getPreferencesConfigFilePath(const std::string& rootPath,
                                                     const std::string& accountId)
{
    if (accountId.empty())
        return rootPath + DIR_SEPARATOR_CH + "data" + DIR_SEPARATOR_CH + "preferences.json";
    else
        return rootPath + DIR_SEPARATOR_CH + "data" + DIR_SEPARATOR_CH + "accountpreferences.json";
}

std::string
PluginPreferencesUtils::valuesFilePath(const std::string& rootPath, const std::string& accountId)
{
    if (accountId.empty() || accountId == "default")
        return rootPath + DIR_SEPARATOR_CH + "preferences.msgpack";
    auto pluginName = rootPath.substr(rootPath.find_last_of(DIR_SEPARATOR_CH) + 1);
    auto dir = fileutils::get_data_dir() + DIR_SEPARATOR_CH + accountId + DIR_SEPARATOR_CH
               + "plugins" + DIR_SEPARATOR_CH + pluginName;
    fileutils::check_dir(dir.c_str());
    return dir + DIR_SEPARATOR_CH + "preferences.msgpack";
}

std::string
PluginPreferencesUtils::getAllowDenyListsPath()
{
    return fileutils::get_data_dir() + DIR_SEPARATOR_CH + "plugins" + DIR_SEPARATOR_CH
           + "allowdeny.msgpack";
}

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

std::map<std::string, std::string>
PluginPreferencesUtils::parsePreferenceConfig(const Json::Value& jsonPreference)
{
    std::map<std::string, std::string> preferenceMap;
    const auto& members = jsonPreference.getMemberNames();
    // Insert other fields
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

std::vector<std::map<std::string, std::string>>
PluginPreferencesUtils::getPreferences(const std::string& rootPath, const std::string& accountId)
{
    std::string preferenceFilePath = getPreferencesConfigFilePath(rootPath, accountId);
    std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferenceFilePath));
    std::ifstream file(preferenceFilePath);
    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    std::set<std::string> keys;
    std::vector<std::map<std::string, std::string>> preferences;
    if (file) {
        // Get preferences locale
        const auto& lang = PluginUtils::getLanguage();
        auto locales = PluginUtils::getLocales(rootPath, std::string(string_remove_suffix(lang, '.')));

        // Read the file to a json format
        bool ok = Json::parseFromStream(rbuilder, file, &root, &errs);
        if (ok && root.isArray()) {
            // Read each preference described in preference.json individually
            for (unsigned i = 0; i < root.size(); i++) {
                const Json::Value& jsonPreference = root[i];
                std::string category = jsonPreference.get("category", "NoCategory").asString();
                std::string type = jsonPreference.get("type", "None").asString();
                std::string key = jsonPreference.get("key", "None").asString();
                // The preference must have at least type and key
                if (type != "None" && key != "None") {
                    if (keys.find(key) == keys.end()) {
                        // Read the rest of the preference
                        auto preferenceAttributes = parsePreferenceConfig(jsonPreference);
                        // If the parsing of the attributes was successful, commit the map and the keys
                        auto defaultValue = preferenceAttributes.find("defaultValue");
                        if (type == "Path" && defaultValue != preferenceAttributes.end()) {
                            // defaultValue in a Path preference is an incomplete path
                            // starting from the installation path of the plugin.
                            // Here we complete the path value.
                            defaultValue->second = rootPath + DIR_SEPARATOR_STR
                                                   + defaultValue->second;
                        }

                        if (!preferenceAttributes.empty()) {
                            for (const auto& locale : locales) {
                                for (auto& pair : preferenceAttributes) {
                                    string_replace(pair.second,
                                                   "{{" + locale.first + "}}",
                                                   locale.second);
                                }
                            }
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

std::map<std::string, std::string>
PluginPreferencesUtils::getUserPreferencesValuesMap(const std::string& rootPath,
                                                    const std::string& accountId)
{
    const std::string preferencesValuesFilePath = valuesFilePath(rootPath, accountId);
    std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferencesValuesFilePath));
    std::ifstream file(preferencesValuesFilePath, std::ios::binary);
    std::map<std::string, std::string> rmap;

    // If file is accessible
    if (file.good()) {
        // Get file size
        std::string str;
        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        // If not empty
        if (fileSize > 0) {
            // Read whole file content and put it in the string str
            str.reserve(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            str.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            try {
                // Unpack the string
                msgpack::object_handle oh = msgpack::unpack(str.data(), str.size());
                // Deserialized object is valid during the msgpack::object_handle instance is alive.
                msgpack::object deserialized = oh.get();
                deserialized.convert(rmap);
            } catch (const std::exception& e) {
                JAMI_ERR() << e.what();
            }
        }
    }
    return rmap;
}

std::map<std::string, std::string>
PluginPreferencesUtils::getPreferencesValuesMap(const std::string& rootPath,
                                                const std::string& accountId)
{
    std::map<std::string, std::string> rmap;

    // Read all preferences values
    std::vector<std::map<std::string, std::string>> preferences = getPreferences(rootPath);
    auto accPrefs = getPreferences(rootPath, accountId);
    for (const auto& item : accPrefs) {
        preferences.push_back(item);
    }
    for (auto& preference : preferences) {
        rmap[preference["key"]] = preference["defaultValue"];
    }

    // If any of these preferences were modified, its value is changed before return
    for (const auto& pair : getUserPreferencesValuesMap(rootPath)) {
        rmap[pair.first] = pair.second;
    }

    if (!accountId.empty()) {
        // If any of these preferences were modified, its value is changed before return
        for (const auto& pair : getUserPreferencesValuesMap(rootPath, accountId)) {
            rmap[pair.first] = pair.second;
        }
    }

    return rmap;
}

bool
PluginPreferencesUtils::resetPreferencesValuesMap(const std::string& rootPath,
                                                  const std::string& accountId)
{
    bool returnValue = true;
    std::map<std::string, std::string> pluginPreferencesMap {};

    const std::string preferencesValuesFilePath = valuesFilePath(rootPath, accountId);
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

void
PluginPreferencesUtils::getAllowDenyListPreferences(ChatHandlerList& list)
{
    const std::string filePath = getAllowDenyListsPath();
    std::lock_guard<std::mutex> guard(fileutils::getFileLock(filePath));
    std::ifstream file(filePath, std::ios::binary);

    // If file is accessible
    if (file.good()) {
        // Get file size
        std::string str;
        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        // If not empty
        if (fileSize > 0) {
            // Read whole file content and put it in the string str
            str.reserve(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            str.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            try {
                // Unpack the string
                msgpack::object_handle oh = msgpack::unpack(str.data(), str.size());
                // Deserialized object is valid during the msgpack::object_handle instance is alive.
                msgpack::object deserialized = oh.get();
                deserialized.convert(list);
            } catch (const std::exception& e) {
                JAMI_ERR() << e.what();
            }
        }
    }
}

void
PluginPreferencesUtils::addAlwaysHandlerPreference(const std::string& handlerName,
                                                   const std::string& rootPath)
{
    {
        std::string filePath = getPreferencesConfigFilePath(rootPath);
        Json::Value root;

        std::lock_guard<std::mutex> guard(fileutils::getFileLock(filePath));
        std::ifstream file(filePath);
        Json::CharReaderBuilder rbuilder;
        Json::Value preference;
        rbuilder["collectComments"] = false;
        std::string errs;
        if (file) {
            bool ok = Json::parseFromStream(rbuilder, file, &root, &errs);
            if (ok && root.isArray()) {
                // Return if preference already exists
                for (const auto& child : root)
                    if (child.get("key", "None").asString() == handlerName + "Always")
                        return;
            }
        }
    }

    std::string filePath = getPreferencesConfigFilePath(rootPath, "acc");
    Json::Value root;
    {
        std::lock_guard<std::mutex> guard(fileutils::getFileLock(filePath));
        std::ifstream file(filePath);
        Json::CharReaderBuilder rbuilder;
        Json::Value preference;
        rbuilder["collectComments"] = false;
        std::string errs;
        if (file) {
            bool ok = Json::parseFromStream(rbuilder, file, &root, &errs);
            if (ok && root.isArray()) {
                // Return if preference already exists
                for (const auto& child : root)
                    if (child.get("key", "None").asString() == handlerName + "Always")
                        return;
            }
        }
        // Create preference structure otherwise
        preference["key"] = handlerName + "Always";
        preference["type"] = "Switch";
        preference["defaultValue"] = "0";
        preference["title"] = "Automatically turn " + handlerName + " on";
        preference["summary"] = handlerName + " will take effect immediately";
        preference["scope"] = "accountId";
        root.append(preference);
    }
    std::lock_guard<std::mutex> guard(fileutils::getFileLock(filePath));
    std::ofstream outFile(filePath);
    if (outFile) {
        // Save preference.json file with new "always preference"
        outFile << root.toStyledString();
        outFile.close();
    }
}

bool
PluginPreferencesUtils::getAlwaysPreference(const std::string& rootPath,
                                            const std::string& handlerName,
                                            const std::string& accountId)
{
    auto preferences = getPreferences(rootPath);
    auto accPrefs = getPreferences(rootPath, accountId);
    for (const auto& item : accPrefs) {
        preferences.push_back(item);
    }
    auto preferencesValues = getPreferencesValuesMap(rootPath, accountId);

    for (const auto& preference : preferences) {
        auto key = preference.at("key");
        if (preference.at("type") == "Switch" && key == handlerName + "Always"
            && preferencesValues.find(key)->second == "1")
            return true;
    }

    return false;
}
} // namespace jami
