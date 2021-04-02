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

#include "media/media_attribute.h"

#include "string_utils.h"
#include "map_utils.h"

namespace jami {

MediaAttribute
MediaAttribute::fromMediaMap(const DRing::MediaMap& mediaMap)
{
    MediaAttribute attr;
    std::string type_str = {};
    jami::parseString(mediaMap, MediaAttributeKey::MEDIA_TYPE, type_str);
    attr.type = stringToMediaType(type_str);

    jami::parseBool(mediaMap, MediaAttributeKey::MUTED, attr.muted);
    jami::parseBool(mediaMap, MediaAttributeKey::ENABLED, attr.enabled);
    jami::parseString(mediaMap, MediaAttributeKey::SOURCE, attr.sourceUri);
    jami::parseString(mediaMap, MediaAttributeKey::LABEL, attr.label);
    return attr;
}

std::vector<MediaAttribute>
MediaAttribute::parseMediaList(const std::vector<DRing::MediaMap>& mediaList)
{
    std::vector<MediaAttribute> mediaAttrList;
    mediaAttrList.reserve(mediaList.size());

    for (auto const& mediaMap : mediaList) {
        mediaAttrList.emplace_back(MediaAttribute::fromMediaMap(mediaMap));
    }

    return mediaAttrList;
}
std::vector<DRing::MediaMap>
MediaAttribute::mediaAttributesToMediaMaps(std::vector<MediaAttribute> mediaAttrList)
{
    std::vector<DRing::MediaMap> mediaList;
    for (auto const& media : mediaAttrList) {
        mediaList.emplace_back(DRing::MediaMap {});
        DRing::MediaMap& mediaMap = mediaList.back();
        const auto* type_str = mediaTypeToString(media.type);
        if (!type_str)
            continue;
        mediaMap.emplace(MediaAttributeKey::MEDIA_TYPE, type_str);
        mediaMap.emplace(MediaAttributeKey::LABEL, media.label);
        mediaMap.emplace(MediaAttributeKey::ENABLED, jami::bool_to_str(media.enabled));
        mediaMap.emplace(MediaAttributeKey::MUTED, jami::bool_to_str(media.muted));
        mediaMap.emplace(MediaAttributeKey::SOURCE, media.sourceUri);
    }

    return mediaList;
}

std::string
MediaAttribute::toString(bool full) const
{
    std::ostringstream descr;
    descr << "[" << this << "] ";
    descr << "type " << mediaTypeToString(type);
    descr << " ";
    descr << "muted " << (muted ? "[YES]" : "[NO]");
    descr << " ";
    descr << "label [" << label << "]";

    if (full) {
        descr << " ";
        descr << "source [" << sourceUri << "]";
        descr << " ";
        descr << "secure " << (secure ? "[YES]" : "[NO]");
    }

    return descr.str();
}
} // namespace jami
