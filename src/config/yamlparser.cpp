/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

namespace jami { namespace yaml_utils {

void
parsePath(const YAML::Node &node, const char *key, std::string& path, const std::string& base)
{
    std::string val;
    parseValue(node, key, val);
    path = fileutils::getCleanPath(base, val);
}

// FIXME: Maybe think of something more clever, this is due to yaml-cpp's poor
// handling of empty values for nested collections.
std::vector<std::map<std::string, std::string>>
parseVectorMap(const YAML::Node &node, const std::initializer_list<std::string> &keys)
{
    std::vector<std::map<std::string, std::string>> result;
    for (const auto &n : node) {
        std::map<std::string, std::string> t;
        for (const auto &k : keys) {
            t[k] = n[k].as<std::string>("");
        }
        result.push_back(t);
    }
    return result;
}

}} // namespace jami::yaml_utils
