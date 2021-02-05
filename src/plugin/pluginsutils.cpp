/*!
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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

namespace jami {

/// DATA_REGEX is used to during the plugin jpl uncompressing
static const std::regex DATA_REGEX("^data" DIR_SEPARATOR_STR_ESC ".+");
/// SO_REGEX is used to find libraries during the plugin jpl uncompressing
static const std::regex SO_REGEX("([a-zA-Z0-9]+(?:[_-]?[a-zA-Z0-9]+)*)" DIR_SEPARATOR_STR_ESC
                                 "([a-zA-Z0-9_-]+\\.(so|dll|lib).*)");

/*!
 * \brief Returns complete manifest.json file path given a installation path.
 * \param rootPath
 */
std::string
PluginUtils::manifestPath(const std::string& rootPath)
{
    return rootPath + DIR_SEPARATOR_CH + "manifest.json";
}

/*!
 * \brief Returns installation path given a plugin's library path.
 * \param soPath
 */
std::string
PluginUtils::getRootPathFromSoPath(const std::string& soPath)
{
    return soPath.substr(0, soPath.find_last_of(DIR_SEPARATOR_CH));
}

/*!
 * \brief Returns data path given a plugin's library path.
 * \param pluginSoPath
 */
std::string
PluginUtils::dataPath(const std::string& pluginSoPath)
{
    return getRootPathFromSoPath(pluginSoPath) + DIR_SEPARATOR_CH + "data";
}

/*!
 * \brief Check if manifest.json has minimum format and parses it's content
 * to a map<string, string>.
 * \param root
 * \return Maps with manifest.json content if success.
 */
std::map<std::string, std::string>
PluginUtils::checkManifestJsonContentValidity(const Json::Value& root)
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

/*!
 * \brief Reads manifest.json stream and checks if it's valid.
 * \param stream
 * \return Maps with manifest.json content if success.
 */
std::map<std::string, std::string>
PluginUtils::checkManifestValidity(std::istream& stream)
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

/*!
 * \brief Recives manifest.json file contents, and checks it's validity.
 * \param vec
 * \return Maps with manifest.json content if success.
 */
std::map<std::string, std::string>
PluginUtils::checkManifestValidity(const std::vector<uint8_t>& vec)
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

/*!
 * \brief Parses the manifest file of an installed plugin if it's valid.
 * \param manifestFilePath
 * \return Map with manifest contents
 */
std::map<std::string, std::string>
PluginUtils::parseManifestFile(const std::string& manifestFilePath)
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

/*!
 * \brief Validates a plugin based on it's manifest.json file.
 * \param rootPath
 * \return True if valid
 */
bool
PluginUtils::checkPluginValidity(const std::string& rootPath)
{
    return !parseManifestFile(manifestPath(rootPath)).empty();
}

/*!
 * \brief Reads the manifest file content without uncompressing the whole archive and
 * return a maps with manifest contents if sucess.
 * \param jplPath
 * \return Map with manifest contents
 */
std::map<std::string, std::string>
PluginUtils::readPluginManifestFromArchive(const std::string& jplPath)
{
    try {
        return checkManifestValidity(archiver::readFileFromArchive(jplPath, "manifest.json"));
    } catch (const std::exception& e) {
        JAMI_ERR() << e.what();
    }
    return {};
}

/*!
 * \brief Function used by archiver to extract files from plugin jpl to the plugin
 * installation path.
 * \param relativeFileName
 * \return Pair <bool, string> meaning if file should be extracted and where to.
 */
std::pair<bool, const std::string>
PluginUtils::uncompressJplFunction(const std::string& relativeFileName)
{
    std::smatch match;
    /// manifest.json and files under data/ folder remains in the same structure
    /// But libraries files are extracted from the folder that matches the running ABI to
    /// the main installation path.
    if (relativeFileName == "manifest.json" || std::regex_match(relativeFileName, DATA_REGEX)) {
        return std::make_pair(true, relativeFileName);
    } else if (regex_search(relativeFileName, match, SO_REGEX)) {
        if (match.str(1) == ABI) {
            return std::make_pair(true, match.str(2));
        }
    }
    return std::make_pair(false, std::string {""});
}
} // namespace jami
