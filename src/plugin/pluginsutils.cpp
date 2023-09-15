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

#include <msgpack.hpp>

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

std::filesystem::path
manifestPath(const std::filesystem::path& rootPath)
{
    return rootPath / "manifest.json";
}

std::map<std::string, std::string>
getPlatformInfo()
{
    return {
        {"os", ABI}
    };
}

std::filesystem::path
getRootPathFromSoPath(const std::filesystem::path& soPath)
{
    return soPath.parent_path();
}

std::filesystem::path
dataPath(const std::filesystem::path& pluginSoPath)
{
    return getRootPathFromSoPath(pluginSoPath) / "data";
}

std::map<std::string, std::string>
checkManifestJsonContentValidity(const Json::Value& root)
{
    std::string name = root.get("name", "").asString();
    std::string id = root.get("id", name).asString();
    std::string description = root.get("description", "").asString();
    std::string version = root.get("version", "").asString();
    std::string iconPath = root.get("iconPath", "icon.png").asString();
    std::string background = root.get("backgroundPath", "background.jpg").asString();
    if (!name.empty() || !version.empty()) {
        return {
                {"id", id},
                {"name", name},
                {"description", description},
                {"version", version},
                {"iconPath", iconPath},
                {"backgroundPath", background},
                };
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
parseManifestFile(const std::filesystem::path& manifestFilePath, const std::string& rootPath)
{
    std::lock_guard<std::mutex> guard(dhtnet::fileutils::getFileLock(manifestFilePath));
    std::ifstream file(manifestFilePath);
    if (file) {
        try {
            const auto& traduction = parseManifestTranslation(rootPath, file);
            return checkManifestValidity(std::vector<uint8_t>(traduction.begin(), traduction.end()));
        } catch (const std::exception& e) {
            JAMI_ERR() << e.what();
        }
    }
    return {};
}

std::string
parseManifestTranslation(const std::string& rootPath, std::ifstream& manifestFile)
{
    if (manifestFile) {
        std::stringstream buffer;
        buffer << manifestFile.rdbuf();
        std::string manifest = buffer.str();
        const auto& translation = getLocales(rootPath, getLanguage());
        std::regex pattern(R"(\{\{([^}]+)\}\})");
        std::smatch matches;
        // replace the pattern to the correct translation
        while (std::regex_search(manifest, matches, pattern)) {
            if (matches.size() == 2) {
                auto it = translation.find(matches[1].str());
                if (it == translation.end()) {
                    manifest = std::regex_replace(manifest, pattern, "");
                    continue;
                }
                manifest = std::regex_replace(manifest, pattern, it->second, std::regex_constants::format_first_only);
            }
        }
        return manifest;
    }
    return {};
}

bool
checkPluginValidity(const std::filesystem::path& rootPath)
{
    return !parseManifestFile(manifestPath(rootPath), rootPath.string()).empty();
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

std::unique_ptr<dht::crypto::Certificate>
readPluginCertificate(const std::string& rootPath, const std::string& pluginId)
{
    std::string certPath = rootPath + DIR_SEPARATOR_CH + pluginId + ".crt";
    try {
        auto cert = fileutils::loadFile(certPath);
        return std::make_unique<dht::crypto::Certificate>(cert);
    } catch (const std::exception& e) {
        JAMI_ERR() << e.what();
    }
    return {};
}

std::unique_ptr<dht::crypto::Certificate>
readPluginCertificateFromArchive(const std::string& jplPath) {
    try {
        auto manifest = readPluginManifestFromArchive(jplPath);
        const std::string& name = manifest["id"];

        if (name.empty()) {
            return {};
        }
        return std::make_unique<dht::crypto::Certificate>(archiver::readFileFromArchive(jplPath, name + ".crt"));
    } catch(const std::exception& e) {
        JAMI_ERR() << e.what();
        return {};
    }
}

std::map<std::string, std::vector<uint8_t>>
readPluginSignatureFromArchive(const std::string& jplPath) {
    try {
        std::vector<uint8_t> vec = archiver::readFileFromArchive(jplPath, "signatures");
        msgpack::object_handle oh = msgpack::unpack(
                        reinterpret_cast<const char*>(vec.data()),
                        vec.size() * sizeof(uint8_t)
                    );
        msgpack::object obj = oh.get();
        return obj.as<std::map<std::string, std::vector<uint8_t>>>();
    } catch(const std::exception& e) {
        JAMI_ERR() << e.what();
        return {};
    }
}

std::vector<uint8_t>
readSignatureFileFromArchive(const std::string& jplPath)
{
    return archiver::readFileFromArchive(jplPath, "signatures.sig");
}

std::pair<bool, std::string_view>
uncompressJplFunction(std::string_view relativeFileName)
{
    std::svmatch match;
    // manifest.json and files under data/ folder remains in the same structure
    // but libraries files are extracted from the folder that matches the running ABI to
    // the main installation path.
    if (std::regex_search(relativeFileName, match, SO_REGEX)) {
        if (std::svsub_match_view(match[1]) != ABI) {
            return std::make_pair(false, std::string_view {});
        } else {
            return std::make_pair(true, std::svsub_match_view(match[2]));
        }
    }
    return std::make_pair(true, relativeFileName);
}

std::string
getLanguage()
{
    std::string lang;
        if (auto envLang = std::getenv("JAMI_LANG"))
            lang = envLang;
        else
            JAMI_INFO() << "Error getting JAMI_LANG env, trying to get system language";
        // If language preference is empty, try to get from the system.
        if (lang.empty()) {
#ifdef WIN32
            WCHAR localeBuffer[LOCALE_NAME_MAX_LENGTH];
            if (GetUserDefaultLocaleName(localeBuffer, LOCALE_NAME_MAX_LENGTH) != 0) {
                char utf8Buffer[LOCALE_NAME_MAX_LENGTH] {};
                WideCharToMultiByte(CP_UTF8,
                                    0,
                                    localeBuffer,
                                    LOCALE_NAME_MAX_LENGTH,
                                    utf8Buffer,
                                    LOCALE_NAME_MAX_LENGTH,
                                    nullptr,
                                    nullptr);

                lang.append(utf8Buffer);
                string_replace(lang, "-", "_");
            }
            // Even though we default to the system variable in windows, technically this
            // part of the code should not be reached because the client-qt must define that
            // variable and we cannot run the client and the daemon in diferent processes in Windows.
#else
            // The same way described in the comment just above, the android should not reach this
            // part of the code given the client-android must define "JAMI_LANG" system variable.
            // And even if this part is reached, it should not work since std::locale is not
            // supported by the NDK.

            // LC_COLLATE is used to grab the locale for the case when the system user has set different
            // values for the preferred Language and Format.
            lang = setlocale(LC_COLLATE, "");
            // We set the environment to avoid checking from system everytime.
            // This is the case when running daemon and client in different processes
            // like with dbus.
            setenv("JAMI_LANG", lang.c_str(), 1);
#endif // WIN32
    }
    return lang;
}

std::map<std::string, std::string>
getLocales(const std::string& rootPath, const std::string& lang)
{
    auto pluginName = rootPath.substr(rootPath.find_last_of(DIR_SEPARATOR_CH) + 1);
    auto basePath = fmt::format("{}/data/locale/{}", rootPath, pluginName + "_");

    std::map<std::string, std::string> locales = {};

    // Get language translations
    if (!lang.empty()) {
        locales = processLocaleFile(basePath + lang + ".json");
    }

    // Get default english values if no translations were found
    if (locales.empty()) {
        locales = processLocaleFile(basePath + "en.json");
    }

    return locales;
}

std::map<std::string, std::string>
processLocaleFile(const std::string& preferenceLocaleFilePath)
{
    if (!std::filesystem::is_regular_file(preferenceLocaleFilePath)) {
        return {};
    }
    std::ifstream file(preferenceLocaleFilePath);
    Json::Value root;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    std::map<std::string, std::string> locales {};
    if (file) {
        // Read the file to a json format
        if (Json::parseFromStream(rbuilder, file, &root, &errs)) {
            auto keys = root.getMemberNames();
            for (const auto& key : keys) {
                locales[key] = root.get(key, "").asString();
            }
        }
    }
    return locales;
}
} // namespace PluginUtils
} // namespace jami
