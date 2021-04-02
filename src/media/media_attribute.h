/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "media/media_codec.h"

namespace jami {

struct MediaAttribute
{
    static MediaAttribute fromMediaMap(const DRing::MediaMap& mediaMap);

    static std::vector<MediaAttribute> parseMediaList(const std::vector<DRing::MediaMap>& mediaList);

    // Serialize a vector of MediaAttribute to a vector of MediaMap
    static std::vector<DRing::MediaMap> mediaAttributesToMediaMaps(
        std::vector<MediaAttribute> mediaAttrList);

    std::string toString(bool full = false) const;

    MediaType type {MediaType::MEDIA_NONE};
    bool muted {false};
    bool secure {true};
    bool enabled {true};
    std::string sourceUri {};
    std::string label {};
};

namespace MediaAttributeKey {
constexpr static char MEDIA_TYPE[] = "MEDIA_TYPE"; // string
constexpr static char ENABLED[] = "ENABLED";       // bool
constexpr static char MUTED[] = "MUTED";           // bool
constexpr static char SOURCE[] = "SOURCE";         // string
constexpr static char LABEL[] = "LABEL";           // string
} // namespace MediaAttributeKey

} // namespace jami
