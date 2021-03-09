/*
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

#pragma once

#include <map>
#include <vector>
#include <json/json.h>

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
 * @brief Function used by archiver to extract files from plugin jpl to the plugin
 * installation path.
 * @param relativeFileName
 * @return Pair <bool, string> meaning if file should be extracted and where to.
 */
std::pair<bool, const std::string> uncompressJplFunction(const std::string& relativeFileName);
} // namespace PluginUtils
} // namespace jami
