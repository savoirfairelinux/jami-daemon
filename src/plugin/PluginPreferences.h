/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

#pragma once

#include <json/json.h>
#include <string>

namespace jami {

std::string getPreferencesConfigFilePathInternal(const std::string& rootPath);

std::string pluginPreferencesValuesFilePathInternal(const std::string& rootPath);

std::string convertArrayToString(const Json::Value& jsonArray);

std::map<std::string, std::string> parsePreferenceConfig(const Json::Value& jsonPreference,
                                                         const std::string& type);

std::vector<std::map<std::string, std::string>> getPluginPreferencesInternal(
    const std::string& rootPath);

std::map<std::string, std::string> getPluginUserPreferencesValuesMapInternal(
    const std::string& rootPath);

std::map<std::string, std::string> getPluginPreferencesValuesMapInternal(const std::string& rootPath);

bool resetPluginPreferencesValuesMapInternal(const std::string& rootPath);
} // namespace jami
