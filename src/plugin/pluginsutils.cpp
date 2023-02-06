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

#include "pluginsutils.h"
#include "logger.h"
#include "fileutils.h"
#include "archiver.h"
#include <fstream>
#include <regex>

#if defined(__APPLE__)
    #if (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        #define ABI "iphone"
    #else
        #define ABI "x86_64-apple-Darwin"
    #endif
#elif defined(__arm__)
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
#elif defined(__aarch64__)
#define ABI "arm64-v8a"
#elif defined(WIN32)
#define ABI "x64-windows"
#else
#define ABI "unknown"
#endif

namespace jami {
namespace PluginUtils {

// DATA_REGEX is used to during the plugin jpl uncompressing
const std::regex DATA_REGEX("^data" DIR_SEPARATOR_STR_ESC ".+");
// SO_REGEX is used to find libraries during the plugin jpl uncompressing
const std::regex SO_REGEX("([a-zA-Z0-9]+(?:[_-]?[a-zA-Z0-9]+)*)" DIR_SEPARATOR_STR_ESC
                          "([a-zA-Z0-9_-]+\\.(dylib|so|dll|lib).*)");

std::string
manifestPath(const std::string& rootPath)
{
    return rootPath + DIR_SEPARATOR_CH + "manifest.json";
}

std::string
getRootPathFromSoPath(const std::string& soPath)
{
    return soPath.substr(0, soPath.find_last_of(DIR_SEPARATOR_CH));
}

std::string
dataPath(const std::string& pluginSoPath)
{
    return getRootPathFromSoPath(pluginSoPath) + DIR_SEPARATOR_CH + "data";
}

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

    if (Json::parseFromStream(rbuilder, stream, &root, &errs)) {
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

std::map<std::string, std::string>
parseManifestFile(const std::string& manifestFilePath)
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

bool
checkPluginValidity(const std::string& rootPath)
{
    return !parseManifestFile(manifestPath(rootPath)).empty();
}

std::map<std::string, std::string>
readPluginManifestFromArchive(const std::string& jplPath)
{
    try {
        return checkManifestValidity(archiver::readFileFromArchive(jplPath, "manifest.json"));
    } catch (const std::exception& e) {
        JAMI_ERR() << e.what();
    }
    return {};
}

std::pair<bool, std::string_view>
uncompressJplFunction(std::string_view relativeFileName)
{
    std::svmatch match;
    // manifest.json and files under data/ folder remains in the same structure
    // but libraries files are extracted from the folder that matches the running ABI to
    // the main installation path.
    if (relativeFileName == "manifest.json" || std::regex_match(relativeFileName, DATA_REGEX)) {
        return std::make_pair(true, relativeFileName);
    } else if (std::regex_search(relativeFileName, match, SO_REGEX)) {
        if (std::svsub_match_view(match[1]) == ABI) {
            return std::make_pair(true, std::svsub_match_view(match[2]));
        }
    }
    return std::make_pair(false, std::string_view {});
}
} // namespace PluginUtils
} // namespace jami
