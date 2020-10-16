/**
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "jamipluginmanager.h"
#include "logger.h"

#include <sstream>
#include <fstream>
#include <regex>
#include <stdexcept>

// Manager
#include "manager.h"
#include "preferences.h"

extern "C" {
#include <archive.h>
}

#include <json/json.h>
#include <msgpack.hpp>

#if defined(__arm__)
#if defined(__ARM_ARCH_7A__)
#define ABI "armeabi-v7a"
#else
#define ABI "armeabi"
#endif
#elif defined(__i386__)
#if __ANDROID__
#define ABI "x86"
#else
#define ABI "x86-linux-gnu"
#endif
#elif defined(__x86_64__)
#if __ANDROID__
#define ABI "x86_64"
#else
#define ABI "x86_64-linux-gnu"
#endif
#elif defined(__mips64) /* mips64el-* toolchain defines __mips__ too */
#define ABI "mips64"
#elif defined(__mips__)
#define ABI "mips"
#elif defined(__aarch64__)
#define ABI "arm64-v8a"
#elif defined(WIN32)
#define ABI "x64-windows"
#else
#define ABI "unknown"
#endif

#define PLUGIN_ALREADY_INSTALLED 100 /* Plugin already installed with the same version */
#define PLUGIN_OLD_VERSION       200 /* Plugin already installed with a newer version */

#ifdef WIN32
#define LIB_TYPE   ".dll"
#define LIB_PREFIX ""
#else
#define LIB_TYPE   ".so"
#define LIB_PREFIX "lib"
#endif

namespace jami {

std::map<std::string, std::string>
checkManifestJsonContentValidity(const Json::Value& root)
{
    std::string name = root.get("name", "").asString();
    std::string description = root.get("description", "").asString();
    std::string version = root.get("version", "").asString();
    if (!name.empty() || !version.empty()) {
        return {{"name", name}, {"description", description}, {"version", version}};
    } else {
        throw std::runtime_error("plugin manifest file: bad format");
    }
}

std::map<std::string, std::string>
checkManifestValidity(std::istream& stream)
{
    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;

    bool ok = Json::parseFromStream(rbuilder, stream, &root, &errs);

    if (ok) {
        return checkManifestJsonContentValidity(root);
    } else {
        throw std::runtime_error("failed to parse the plugin manifest file");
    }
}

std::map<std::string, std::string>
checkManifestValidity(const std::vector<uint8_t>& vec)
{
    Json::Value root;
    std::unique_ptr<Json::CharReader> json_Reader(Json::CharReaderBuilder {}.newCharReader());
    std::string errs;

    bool ok = json_Reader->parse(reinterpret_cast<const char*>(vec.data()),
                                 reinterpret_cast<const char*>(vec.data() + vec.size()),
                                 &root,
                                 &errs);

    if (ok) {
        return checkManifestJsonContentValidity(root);
    } else {
        throw std::runtime_error("failed to parse the plugin manifest file");
    }
}

static const std::regex DATA_REGEX("^data" DIR_SEPARATOR_STR_ESC ".+");
static const std::regex SO_REGEX("([a-zA-Z0-9]+(?:[_-]?[a-zA-Z0-9]+)*)" DIR_SEPARATOR_STR_ESC
                                 "([a-zA-Z0-9_-]+\\.(so|dll|lib).*)");

std::pair<bool, const std::string>
uncompressJplFunction(const std::string& relativeFileName)
{
    std::smatch match;
    if (relativeFileName == "manifest.json" || std::regex_match(relativeFileName, DATA_REGEX)) {
        return std::make_pair(true, relativeFileName);
    } else if (regex_search(relativeFileName, match, SO_REGEX) == true) {
        if (match.str(1) == ABI) {
            return std::make_pair(true, match.str(2));
        }
    }
    return std::make_pair(false, std::string {""});
}

std::string
convertArrayToString(const Json::Value& jsonArray)
{
    std::string stringArray = "";

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
parsePreferenceConfig(const Json::Value& jsonPreference, const std::string& type)
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

std::map<std::string, std::string>
JamiPluginManager::getPluginDetails(const std::string& rootPath)
{
    auto detailsIt = pluginDetailsMap_.find(rootPath);
    if (detailsIt != pluginDetailsMap_.end()) {
        return detailsIt->second;
    }

    std::map<std::string, std::string> details = parseManifestFile(manifestPath(rootPath));
    if (!details.empty()) {
        details["iconPath"] = rootPath + DIR_SEPARATOR_CH + "data" + DIR_SEPARATOR_CH + "icon.png";
        details["soPath"] = rootPath + DIR_SEPARATOR_CH + LIB_PREFIX + details["name"] + LIB_TYPE;
        detailsIt = pluginDetailsMap_.emplace(rootPath, std::move(details)).first;
        return detailsIt->second;
    }
    return {};
}

std::vector<std::string>
JamiPluginManager::listAvailablePlugins()
{
    std::string pluginsPath = fileutils::get_data_dir() + DIR_SEPARATOR_CH + "plugins";
    std::vector<std::string> pluginsPaths = fileutils::readDirectory(pluginsPath);
    std::for_each(pluginsPaths.begin(), pluginsPaths.end(), [&pluginsPath](std::string& x) {
        x = pluginsPath + DIR_SEPARATOR_CH + x;
    });
    auto predicate = [this](std::string path) {
        return !checkPluginValidity(path);
    };
    auto returnIterator = std::remove_if(pluginsPaths.begin(), pluginsPaths.end(), predicate);
    pluginsPaths.erase(returnIterator, std::end(pluginsPaths));
    return pluginsPaths;
}

int
JamiPluginManager::installPlugin(const std::string& jplPath, bool force)
{
    int r {0};
    if (fileutils::isFile(jplPath)) {
        try {
            auto manifestMap = readPluginManifestFromArchive(jplPath);
            std::string name = manifestMap["name"];
            if (name.empty())
                return 0;
            std::string version = manifestMap["version"];
            const std::string destinationDir {fileutils::get_data_dir() + DIR_SEPARATOR_CH
                                              + "plugins" + DIR_SEPARATOR_CH + name};
            // Find if there is an existing version of this plugin
            const auto alreadyInstalledManifestMap = parseManifestFile(manifestPath(destinationDir));

            if (!alreadyInstalledManifestMap.empty()) {
                if (force) {
                    r = uninstallPlugin(destinationDir);
                    if (r == 0) {
                        archiver::uncompressArchive(jplPath, destinationDir, uncompressJplFunction);
                    }
                } else {
                    std::string installedVersion = alreadyInstalledManifestMap.at("version");
                    if (version > installedVersion) {
                        r = uninstallPlugin(destinationDir);
                        if (r == 0) {
                            archiver::uncompressArchive(jplPath,
                                                        destinationDir,
                                                        uncompressJplFunction);
                        }
                    } else if (version == installedVersion) {
                        // An error code of 100 to know that this version is the same as the one installed
                        r = PLUGIN_ALREADY_INSTALLED;
                    } else {
                        // An error code of 100 to know that this version is older than the one installed
                        r = PLUGIN_OLD_VERSION;
                    }
                }
            } else {
                archiver::uncompressArchive(jplPath, destinationDir, uncompressJplFunction);
            }
        } catch (const std::exception& e) {
            JAMI_ERR() << e.what();
        }
    }
    return r;
}

int
JamiPluginManager::uninstallPlugin(const std::string& rootPath)
{
    if (checkPluginValidity(rootPath)) {
        auto detailsIt = pluginDetailsMap_.find(rootPath);
        if (detailsIt != pluginDetailsMap_.end()) {
            bool loaded = pm_.checkLoadedPlugin(detailsIt->second.at("soPath"));
            if (loaded) {
                JAMI_INFO() << "PLUGIN: unloading before uninstall.";
                bool status = unloadPlugin(rootPath);
                if (!status) {
                    JAMI_INFO() << "PLUGIN: could not unload, not performing uninstall.";
                    return -1;
                }
            }
            pluginDetailsMap_.erase(detailsIt);
        }
        return fileutils::removeAll(rootPath);
    } else {
        JAMI_INFO() << "PLUGIN: not installed.";
        return -1;
    }
}

bool
JamiPluginManager::loadPlugin(const std::string& rootPath)
{
#ifdef ENABLE_PLUGIN
    try {
        bool status = pm_.load(getPluginDetails(rootPath).at("soPath"));
        JAMI_INFO() << "PLUGIN: load status - " << status;

        return status;

    } catch (const std::exception& e) {
        JAMI_ERR() << e.what();
        return false;
    }
#endif
    return false;
}

bool
JamiPluginManager::unloadPlugin(const std::string& rootPath)
{
#ifdef ENABLE_PLUGIN
    try {
        bool status = pm_.unload(getPluginDetails(rootPath).at("soPath"));
        JAMI_INFO() << "PLUGIN: unload status - " << status;

        return status;
    } catch (const std::exception& e) {
        JAMI_ERR() << e.what();
        return false;
    }
#endif
    return false;
}

void
JamiPluginManager::togglePlugin(const std::string& rootPath, bool toggle)
{
    // This function should not be used as is
    // One should modify it to perform plugin install followed by load
    // rootPath should be the jplpath!
    try {
        std::string soPath = getPluginDetails(rootPath).at("soPath");
        // remove the previous plugin object if it was registered
        pm_.destroyPluginComponents(soPath);
        // If toggle, register a new instance of the plugin
        // function
        if (toggle) {
            pm_.callPluginInitFunction(soPath);
        }
    } catch (const std::exception& e) {
        JAMI_ERR() << e.what();
    }
}

std::vector<std::string>
JamiPluginManager::listLoadedPlugins() const
{
    std::vector<std::string> loadedSoPlugins = pm_.listLoadedPlugins();
    std::vector<std::string> loadedPlugins {};
    loadedPlugins.reserve(loadedSoPlugins.size());
    std::transform(loadedSoPlugins.begin(),
                   loadedSoPlugins.end(),
                   std::back_inserter(loadedPlugins),
                   [this](const std::string& soPath) { return getRootPathFromSoPath(soPath); });
    return loadedPlugins;
}

std::vector<std::map<std::string, std::string>>
JamiPluginManager::getPluginPreferences(const std::string& rootPath)
{
    const std::string preferenceFilePath = getPreferencesConfigFilePath(rootPath);
    std::ifstream file(preferenceFilePath);
    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    std::set<std::string> keys;
    std::vector<std::map<std::string, std::string>> preferences;
    if (file) {
        bool ok = Json::parseFromStream(rbuilder, file, &root, &errs);
        if (ok && root.isArray()) {
            for (unsigned i = 0; i < root.size(); i++) {
                const Json::Value& jsonPreference = root[i];
                std::string category = jsonPreference.get("category", "NoCategory").asString();
                std::string type = jsonPreference.get("type", "None").asString();
                std::string key = jsonPreference.get("key", "None").asString();
                if (type != "None" && key != "None") {
                    if (keys.find(key) == keys.end()) {
                        auto preferenceAttributes = parsePreferenceConfig(jsonPreference, type);
                        // If the parsing of the attributes was successful, commit the map and the keys
                        auto defaultValue = preferenceAttributes.find("defaultValue");
                        if (type == "Path" && defaultValue != preferenceAttributes.end()) {
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

std::map<std::string, std::string>
JamiPluginManager::getPluginUserPreferencesValuesMap(const std::string& rootPath)
{
    const std::string preferencesValuesFilePath = pluginPreferencesValuesFilePath(rootPath);
    std::ifstream file(preferencesValuesFilePath, std::ios::binary);
    std::map<std::string, std::string> rmap;

    // If file is accessible
    if (file.good()) {
        std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferencesValuesFilePath));
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

bool
JamiPluginManager::setPluginPreference(const std::string& rootPath,
                                       const std::string& key,
                                       const std::string& value)
{
    std::map<std::string, std::string> pluginUserPreferencesMap = getPluginUserPreferencesValuesMap(
        rootPath);
    std::map<std::string, std::string> pluginPreferencesMap = getPluginPreferencesValuesMap(
        rootPath);

    auto find = pluginPreferencesMap.find(key);
    if (find != pluginPreferencesMap.end()) {
        std::vector<std::map<std::string, std::string>> preferences = getPluginPreferences(rootPath);
        for (auto& preference : preferences) {
            if (!preference["key"].compare(key)) {
                csm_.setPreference(key, value, preference["scope"]);
                break;
            }
        }

        pluginUserPreferencesMap[key] = value;
        const std::string preferencesValuesFilePath = pluginPreferencesValuesFilePath(rootPath);
        std::ofstream fs(preferencesValuesFilePath, std::ios::binary);
        if (!fs.good()) {
            return false;
        }
        try {
            std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferencesValuesFilePath));
            msgpack::pack(fs, pluginUserPreferencesMap);
            return true;
        } catch (const std::exception& e) {
            JAMI_ERR() << e.what();
            return false;
        }
    }
    return false;
}

std::map<std::string, std::string>
JamiPluginManager::getPluginPreferencesValuesMap(const std::string& rootPath)
{
    std::map<std::string, std::string> rmap;

    std::vector<std::map<std::string, std::string>> preferences = getPluginPreferences(rootPath);
    for (auto& preference : preferences) {
        rmap[preference["key"]] = preference["defaultValue"];
    }

    for (const auto& pair : getPluginUserPreferencesValuesMap(rootPath)) {
        rmap[pair.first] = pair.second;
    }
    return rmap;
}

bool
JamiPluginManager::resetPluginPreferencesValuesMap(const std::string& rootPath)
{
    bool returnValue = true;
    std::map<std::string, std::string> pluginPreferencesMap {};

    const std::string preferencesValuesFilePath = pluginPreferencesValuesFilePath(rootPath);
    std::ofstream fs(preferencesValuesFilePath, std::ios::binary);
    if (!fs.good()) {
        return false;
    }
    try {
        std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferencesValuesFilePath));
        msgpack::pack(fs, pluginPreferencesMap);
    } catch (const std::exception& e) {
        returnValue = false;
        JAMI_ERR() << e.what();
    }

    return returnValue;
}

std::map<std::string, std::string>
JamiPluginManager::readPluginManifestFromArchive(const std::string& jplPath)
{
    try {
        return checkManifestValidity(archiver::readFileFromArchive(jplPath, "manifest.json"));
    } catch (const std::exception& e) {
        JAMI_ERR() << e.what();
    }
    return {};
}

std::map<std::string, std::string>
JamiPluginManager::parseManifestFile(const std::string& manifestFilePath)
{
    std::ifstream file(manifestFilePath);
    if (file) {
        try {
            return checkManifestValidity(file);
        } catch (const std::exception& e) {
            JAMI_ERR() << e.what();
        }
    }
    return {};
}

void
JamiPluginManager::registerServices()
{
    // Register pluginPreferences
    pm_.registerService("getPluginPreferences", [this](const DLPlugin* plugin, void* data) {
        auto ppp = static_cast<std::map<std::string, std::string>*>(data);
        *ppp = getPluginPreferencesValuesMap(getRootPathFromSoPath(plugin->getPath()));
        return 0;
    });

    pm_.registerService("getPluginDataPath", [this](const DLPlugin* plugin, void* data) {
        auto dataPath_ = static_cast<std::string*>(data);
        dataPath_->assign(dataPath(plugin->getPath()));
        return 0;
    });
}

} // namespace jami
