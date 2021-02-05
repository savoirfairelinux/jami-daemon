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

#pragma once

#include <map>
#include <vector>
#include <json/json.h>

namespace jami {

/*! \class  PluginUtils
 * \brief This class provides auxiliar functions to the Plugin System.
 * Specially to the JamiPluginManager class.
 * Those functions were originaly part of the latter class, but for
 * code clearity purposes, they were moved.
 */
class PluginUtils
{
public:
    static std::string manifestPath(const std::string& rootPath);

    static std::string getRootPathFromSoPath(const std::string& soPath);

    static std::string dataPath(const std::string& pluginSoPath);

    static std::map<std::string, std::string> checkManifestJsonContentValidity(
        const Json::Value& root);

    static std::map<std::string, std::string> checkManifestValidity(std::istream& stream);

    static std::map<std::string, std::string> checkManifestValidity(const std::vector<uint8_t>& vec);

    static std::map<std::string, std::string> parseManifestFile(const std::string& manifestFilePath);

    static bool checkPluginValidity(const std::string& rootPath);

    static std::map<std::string, std::string> readPluginManifestFromArchive(
        const std::string& jplPath);

    static std::pair<bool, const std::string> uncompressJplFunction(
        const std::string& relativeFileName);

private:
    PluginUtils() {}
    ~PluginUtils() {}
};
} // namespace jami
