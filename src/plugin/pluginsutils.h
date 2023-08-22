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

#pragma once

#include <map>
#include <vector>

#include <json/json.h>
#include <opendht/crypto.h>

namespace jami {

/**
 * @namespace  PluginUtils
 * @brief This namespace provides auxiliary functions to the Plugin System.
 * Specially to the JamiPluginManager class.
 * Those functions were originally part of the latter class, but for
 * code clarity purposes, they were moved.
 */
namespace PluginUtils {
/**
 * @brief Returns complete manifest.json file path given a installation path.
 * @param rootPath
 */
std::string manifestPath(const std::string& rootPath);

/**
 * @brief Returns installation path given a plugin's library path.
 * @param soPath
 */
std::string getRootPathFromSoPath(const std::string& soPath);

/**
 * @brief Returns data path given a plugin's library path.
 * @param pluginSoPath
 */
std::string dataPath(const std::string& pluginSoPath);

/**
 * @brief Check if manifest.json has minimum format and parses its content
 * to a map<string, string>.
 * @param root
 * @return Maps with manifest.json content if success.
 */
std::map<std::string, std::string> checkManifestJsonContentValidity(const Json::Value& root);

/**
 * @brief Reads manifest.json stream and checks if it's valid.
 * @param stream
 * @return Maps with manifest.json content if success.
 */
std::map<std::string, std::string> checkManifestValidity(std::istream& stream);

/**
 * @brief Recives manifest.json file contents, and checks its validity.
 * @param vec
 * @return Maps with manifest.json content if success.
 */
std::map<std::string, std::string> checkManifestValidity(const std::vector<uint8_t>& vec);

/**
 * @brief Returns a map with platform information.
 * @return Map with platform information
*/
std::map<std::string, std::string> getPlatformInfo();

/**
 * @brief Parses the manifest file of an installed plugin if it's valid.
 * @param manifestFilePath
 * @return Map with manifest contents
 */
std::map<std::string, std::string> parseManifestFile(const std::string& manifestFilePath);

/**
 * @brief Validates a plugin based on its manifest.json file.
 * @param rootPath
 * @return True if valid
 */
bool checkPluginValidity(const std::string& rootPath);

/**
 * @brief Reads the manifest file content without uncompressing the whole archive and
 * return a map with manifest contents if success.
 * @param jplPath
 * @return Map with manifest contents
 */
std::map<std::string, std::string> readPluginManifestFromArchive(const std::string& jplPath);

/**
 * @brief Read the plugin's certificate
 * @param rootPath
 * @param pluginId
 * @return Certificate object pointer
*/
std::unique_ptr<dht::crypto::Certificate> readPluginCertificate(const std::string& rootPath, const std::string& pluginId);
/**
 * @brief Read plugin certificate without uncompressing the whole archive.and
 * return an object Certificate
 * @param jplPath
 * @return Certificate object pointer
 */
std::unique_ptr<dht::crypto::Certificate> readPluginCertificateFromArchive(const std::string& jplPath);

/**
 * @brief Reads signature file content without uncompressing the whole archive and
 * @param jplPath
 * return a map of signature path as key and signature content as value.
*/
std::map<std::string, std::vector<uint8_t>> readPluginSignatureFromArchive(const std::string& jplPath);

/**
 * @brief Read the signature of the file signature without uncompressing the whole archive.
 * @param jplPath
 * @return Signature file content
*/
std::vector<uint8_t> readSignatureFileFromArchive(const std::string& jplPath);

/**
 * @brief Function used by archiver to extract files from plugin jpl to the plugin
 * installation path.
 * @param relativeFileName
 * @return Pair <bool, string> meaning if file should be extracted and where to.
 */
std::pair<bool, std::string_view> uncompressJplFunction(std::string_view relativeFileName);
} // namespace PluginUtils
} // namespace jami
