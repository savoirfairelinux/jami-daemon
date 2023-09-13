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

#include "yamlparser.h"
#include "fileutils.h"

namespace jami {
namespace yaml_utils {

void
parsePath(const YAML::Node& node, const char* key, std::string& path, const std::string& base)
{
    std::string val;
    parseValue(node, key, val);
    path = fileutils::getFullPath(base, val).string();
}

void
parsePathOptional(const YAML::Node& node, const char* key, std::string& path, const std::string& base)
{
    std::string val;
    if (parseValueOptional(node, key, val))
        path = fileutils::getFullPath(base, val).string();
}

std::vector<std::map<std::string, std::string>>
parseVectorMap(const YAML::Node& node, const std::initializer_list<std::string>& keys)
{
    std::vector<std::map<std::string, std::string>> result;
    result.reserve(node.size());
    for (const auto& n : node) {
        std::map<std::string, std::string> t;
        for (const auto& k : keys) {
            t[k] = n[k].as<std::string>("");
        }
        result.emplace_back(std::move(t));
    }
    return result;
}

std::set<std::string>
parseVector(const YAML::Node& node)
{
    std::set<std::string> result;
    for (const auto& n : node) {
        result.emplace(n.as<std::string>(""));
    }
    return result;
}
} // namespace yaml_utils
} // namespace jami
