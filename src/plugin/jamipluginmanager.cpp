/**
 *  Copyright (C)2020-2021 Savoir-faire Linux Inc.
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
#include <msgpack.hpp>
#include "manager.h"
#include "preferences.h"

extern "C" {
#include <archive.h>
}

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
    std::string iconPath = root.get("iconPath", "icon.png").asString();
    if (!name.empty() || !version.empty()) {
        return {{"name", name},
                {"description", description},
                {"version", version},
                {"iconPath", iconPath}};
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

std::map<std::string, std::string>
JamiPluginManager::getPluginDetails(const std::string& rootPath)
{
    auto detailsIt = pluginDetailsMap_.find(rootPath);
    if (detailsIt != pluginDetailsMap_.end()) {
        return detailsIt->second;
    }

    std::map<std::string, std::string> details = parseManifestFile(manifestPath(rootPath));
    if (!details.empty()) {
        auto it = details.find("iconPath");
        it->second.insert(0, rootPath + DIR_SEPARATOR_CH + "data" + DIR_SEPARATOR_CH);
        details["soPath"] = rootPath + DIR_SEPARATOR_CH + LIB_PREFIX + details["name"] + LIB_TYPE;
        detailsIt = pluginDetailsMap_.emplace(rootPath, std::move(details)).first;
        return detailsIt->second;
    }
    return {};
}

std::vector<std::string>
JamiPluginManager::getInstalledPlugins()
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

    std::vector<std::string> nonStandardInstalls = jami::Manager::instance()
                                                       .pluginPreferences.getInstalledPlugins();
    for (auto& path : nonStandardInstalls) {
        if (checkPluginValidity(path))
            pluginsPaths.emplace_back(path);
    }

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
            loadPlugin(destinationDir);
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
            bool loaded = pm_.checkLoadedPlugin(rootPath);
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

std::vector<std::string>
JamiPluginManager::getLoadedPlugins() const
{
    std::vector<std::string> loadedSoPlugins = pm_.getLoadedPlugins();
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
    return PluginPreferencesUtils::getPreferences(rootPath);
}

std::map<std::string, std::string>
JamiPluginManager::getPluginUserPreferencesValuesMap(const std::string& rootPath)
{
    return PluginPreferencesUtils::getUserPreferencesValuesMap(rootPath);
}

bool
JamiPluginManager::setPluginPreference(const std::string& rootPath,
                                       const std::string& key,
                                       const std::string& value)
{
    std::map<std::string, std::string> pluginUserPreferencesMap
        = PluginPreferencesUtils::getUserPreferencesValuesMap(rootPath);
    std::map<std::string, std::string> pluginPreferencesMap
        = PluginPreferencesUtils::getPreferencesValuesMap(rootPath);

    std::vector<std::map<std::string, std::string>> preferences
        = PluginPreferencesUtils::getPreferences(rootPath);
    bool force {pm_.checkLoadedPlugin(rootPath)};

    for (auto& preference : preferences) {
        if (!preference["key"].compare(key)) {
            force &= callsm_.setPreference(key, value, rootPath);
            force &= chatsm_.setPreference(key, value, rootPath);
            break;
        }
    }
    if (force)
        unloadPlugin(rootPath);

    auto find = pluginPreferencesMap.find(key);
    if (find != pluginPreferencesMap.end()) {
        pluginUserPreferencesMap[key] = value;
        const std::string preferencesValuesFilePath = PluginPreferencesUtils::valuesFilePath(
            rootPath);
        std::lock_guard<std::mutex> guard(fileutils::getFileLock(preferencesValuesFilePath));
        std::ofstream fs(preferencesValuesFilePath, std::ios::binary);
        if (!fs.good()) {
            if (force) {
                loadPlugin(rootPath);
            }
            return false;
        }
        try {
            msgpack::pack(fs, pluginUserPreferencesMap);
        } catch (const std::exception& e) {
            JAMI_ERR() << e.what();
            if (force) {
                loadPlugin(rootPath);
            }
            return false;
        }
    }
    if (force) {
        loadPlugin(rootPath);
    }
    return true;
}

std::map<std::string, std::string>
JamiPluginManager::getPluginPreferencesValuesMap(const std::string& rootPath)
{
    return PluginPreferencesUtils::getPreferencesValuesMap(rootPath);
}

bool
JamiPluginManager::resetPluginPreferencesValuesMap(const std::string& rootPath)
{
    bool loaded {pm_.checkLoadedPlugin(rootPath)};
    if (loaded)
        unloadPlugin(rootPath);
    auto status = PluginPreferencesUtils::resetPreferencesValuesMap(rootPath);
    if (loaded) {
        loadPlugin(rootPath);
    }
    return status;
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
    std::lock_guard<std::mutex> guard(fileutils::getFileLock(manifestFilePath));
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
