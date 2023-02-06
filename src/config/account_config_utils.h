/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "yamlparser.h"

template<typename T>
inline void serializeValue(YAML::Emitter& out, const char* key, const T& value, const T& def) {
    if (value != def)
        out << YAML::Key << key << YAML::Value << value;
}

#define SERIALIZE_CONFIG(key, name) serializeValue(out, key, name, DEFAULT_CONFIG.name)
#define SERIALIZE_PATH(key, name) serializeValue(out, key, fileutils::getCleanPath(path, name), DEFAULT_CONFIG.name)
