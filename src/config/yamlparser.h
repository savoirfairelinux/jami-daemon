/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Authors: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *           Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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

#include "logger.h"
#include <yaml-cpp/yaml.h>

namespace jami {
namespace yaml_utils {

// set T to the value stored at key, or leaves T unchanged
// if no value is stored.
template<typename T>
void
parseValue(const YAML::Node& node, const char* key, T& value)
{
    value = node[key].as<T>();
}

template<typename T>
bool
parseValueOptional(const YAML::Node& node, const char* key, T& value)
{
    try {
        parseValue(node, key, value);
        return true;
    } catch (const std::exception& e) {
        // JAMI_DBG("Can't read yaml field: %s", key);
    }
    return false;
}

void parsePath(const YAML::Node& node, const char* key, std::string& path, const std::filesystem::path& base);
void parsePathOptional(const YAML::Node& node, const char* key, std::string& path, const std::filesystem::path& base);

std::vector<std::map<std::string, std::string>> parseVectorMap(
    const YAML::Node& node, const std::initializer_list<std::string>& keys);
std::set<std::string> parseVector(const YAML::Node& node);

} // namespace yaml_utils
} // namespace jami
