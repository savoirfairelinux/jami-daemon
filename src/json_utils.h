/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <string_view>
#include <json/json.h>
#include <logger.h>

namespace jami {

extern const Json::CharReaderBuilder rbuilder;

inline bool parseJson(std::string_view jsonStr, Json::Value& json) {
    std::string err;
    auto reader = std::unique_ptr<Json::CharReader>(rbuilder.newCharReader());
    if (!reader->parse(jsonStr.data(),
                       jsonStr.data() + jsonStr.size(),
                       &json,
                       &err)) {
        JAMI_WARNING("Can't parse JSON: {}\n{}", err, jsonStr);
        return false;
    }
    return true;
}

}
