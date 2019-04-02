/*
 *  Copyright (C) 2014-2019 Savoir-faire Linux Inc.
 *  Author : Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "string_utils.h"

#include <msgpack.hpp>
#include <json/json.h>

#include <map>
#include <string>
#include <ctime>
#include <ciso646>

namespace jami {

struct Contact
{
    /** Time of contact addition */
    time_t added {0};

    /** Time of contact removal */
    time_t removed {0};

    /** True if we got confirmation that this contact also added us */
    bool confirmed {false};

    /** True if the contact is banned (if not active) */
    bool banned {false};

    /** True if the contact is an active contact (not banned nor removed) */
    bool isActive() const { return added > removed; }
    bool isBanned() const { return not isActive() and banned; }

    Contact() = default;
    Contact(const Json::Value& json) {
        added = json["added"].asInt();
        removed = json["removed"].asInt();
        confirmed = json["confirmed"].asBool();
        banned = json["banned"].asBool();
    }

    /**
     * Update this contact using other known contact information,
     * return true if contact state was changed.
     */
    bool update(const Contact& c) {
        const auto copy = *this;
        if (c.added > added) {
            added = c.added;
        }
        if (c.removed > removed) {
            removed = c.removed;
            banned = c.banned;
        }
        if (c.confirmed != confirmed) {
            confirmed = c.confirmed or confirmed;
        }
        return hasDifferentState(copy);
    }
    bool hasDifferentState(const Contact& other) const {
        return other.isActive() != isActive()
            or other.isBanned() != isBanned()
            or other.confirmed  != confirmed;
    }

    Json::Value toJson() const {
        Json::Value json;
        json["added"] = Json::Int64(added);
        json["removed"] = Json::Int64(removed);
        json["confirmed"] = confirmed;
        json["banned"] = banned;
        return json;
    }

    std::map<std::string, std::string> toMap() const {
        if (not (isActive() or isBanned())) {
            return {};
        }

        std::map<std::string, std::string> result {
            {"added", std::to_string(added)}
        };

        if (isActive())
            result.emplace("confirmed", confirmed ? TRUE_STR : FALSE_STR);
        else if (isBanned())
            result.emplace("banned", TRUE_STR);

        return result;
    }

    MSGPACK_DEFINE_MAP(added, removed, confirmed, banned)
};

}
