/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
 *  Author: Mohamed Chibani <mohamed.chibani@savoirfairelinux.com>
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
#include "jami/media_const.h"

namespace jami {

MediaAttribute::MediaAttribute(const libjami::MediaMap& mediaMap, bool secure)
{
    std::pair<bool, MediaType> pairType = getMediaType(mediaMap);
    if (pairType.first)
        type_ = pairType.second;

    std::pair<bool, bool> pairBool;

    pairBool = getBoolValue(mediaMap, libjami::Media::MediaAttributeKey::MUTED);
    if (pairBool.first)
        muted_ = pairBool.second;

    pairBool = getBoolValue(mediaMap, libjami::Media::MediaAttributeKey::ENABLED);
    if (pairBool.first)
        enabled_ = pairBool.second;

    std::pair<bool, std::string> pairString;
    pairString = getStringValue(mediaMap, libjami::Media::MediaAttributeKey::SOURCE);
    if (pairBool.first)
        sourceUri_ = pairString.second;

    pairString = getStringValue(mediaMap, libjami::Media::MediaAttributeKey::LABEL);
    if (pairBool.first)
        label_ = pairString.second;

    pairBool = getBoolValue(mediaMap, libjami::Media::MediaAttributeKey::ON_HOLD);
    if (pairBool.first)
        onHold_ = pairBool.second;

    secure_ = secure;
}

std::vector<MediaAttribute>
MediaAttribute::buildMediaAttributesList(const std::vector<libjami::MediaMap>& mediaList, bool secure)
{
    std::vector<MediaAttribute> mediaAttrList;
    mediaAttrList.reserve(mediaList.size());

    for (auto const& mediaMap : mediaList) {
        mediaAttrList.emplace_back(MediaAttribute(mediaMap, secure));
    }

    return mediaAttrList;
}

MediaType
MediaAttribute::stringToMediaType(const std::string& mediaType)
{
    if (mediaType.compare(libjami::Media::MediaAttributeValue::AUDIO) == 0)
        return MediaType::MEDIA_AUDIO;
    if (mediaType.compare(libjami::Media::MediaAttributeValue::VIDEO) == 0)
        return MediaType::MEDIA_VIDEO;
    return MediaType::MEDIA_NONE;
}

std::pair<bool, MediaType>
MediaAttribute::getMediaType(const libjami::MediaMap& map)
{
    const auto& iter = map.find(libjami::Media::MediaAttributeKey::MEDIA_TYPE);
    if (iter == map.end()) {
        return {false, MediaType::MEDIA_NONE};
    }

    auto type = stringToMediaType(iter->second);
    if (type == MediaType::MEDIA_NONE) {
        JAMI_ERR("Invalid value [%s] for a media type key in media map", iter->second.c_str());
        return {false, type};
    }

    return {true, type};
}

std::pair<bool, bool>
MediaAttribute::getBoolValue(const libjami::MediaMap& map, const std::string& key)
{
    const auto& iter = map.find(key);
    if (iter == map.end()) {
        return {false, false};
    }

    auto const& value = iter->second;
    if (value.compare(TRUE_STR) == 0)
        return {true, true};
    if (value.compare(FALSE_STR) == 0)
        return {true, false};

    JAMI_ERR("Invalid value %s for a boolean key", value.c_str());
    return {false, false};
}

std::pair<bool, std::string>
MediaAttribute::getStringValue(const libjami::MediaMap& map, const std::string& key)
{
    const auto& iter = map.find(key);
    if (iter == map.end()) {
        return {false, {}};
    }

    return {true, iter->second};
}

char const*
MediaAttribute::boolToString(bool val)
{
    return val ? TRUE_STR : FALSE_STR;
}

char const*
MediaAttribute::mediaTypeToString(MediaType type)
{
    if (type == MediaType::MEDIA_AUDIO)
        return libjami::Media::MediaAttributeValue::AUDIO;
    if (type == MediaType::MEDIA_VIDEO)
        return libjami::Media::MediaAttributeValue::VIDEO;
    return nullptr;
}

bool
MediaAttribute::hasMediaType(const std::vector<MediaAttribute>& mediaList, MediaType type)
{
    return mediaList.end()
           != std::find_if(mediaList.begin(), mediaList.end(), [type](const MediaAttribute& media) {
                  return media.type_ == type;
              });
}

libjami::MediaMap
MediaAttribute::toMediaMap(const MediaAttribute& mediaAttr)
{
    libjami::MediaMap mediaMap;

    mediaMap.emplace(libjami::Media::MediaAttributeKey::MEDIA_TYPE,
                     mediaTypeToString(mediaAttr.type_));
    mediaMap.emplace(libjami::Media::MediaAttributeKey::LABEL, mediaAttr.label_);
    mediaMap.emplace(libjami::Media::MediaAttributeKey::ENABLED, boolToString(mediaAttr.enabled_));
    mediaMap.emplace(libjami::Media::MediaAttributeKey::MUTED, boolToString(mediaAttr.muted_));
    mediaMap.emplace(libjami::Media::MediaAttributeKey::SOURCE, mediaAttr.sourceUri_);
    mediaMap.emplace(libjami::Media::MediaAttributeKey::ON_HOLD, boolToString(mediaAttr.onHold_));

    return mediaMap;
}

std::vector<libjami::MediaMap>
MediaAttribute::mediaAttributesToMediaMaps(std::vector<MediaAttribute> mediaAttrList)
{
    std::vector<libjami::MediaMap> mediaList;
    mediaAttrList.reserve(mediaAttrList.size());
    for (auto const& media : mediaAttrList) {
        mediaList.emplace_back(toMediaMap(media));
    }

    return mediaList;
}

std::string
MediaAttribute::toString(bool full) const
{
    std::ostringstream descr;
    descr << "type " << (type_ == MediaType::MEDIA_AUDIO ? "[AUDIO]" : "[VIDEO]");
    descr << " ";
    descr << "enabled " << (enabled_ ? "[YES]" : "[NO]");
    descr << " ";
    descr << "muted " << (muted_ ? "[YES]" : "[NO]");
    descr << " ";
    descr << "label [" << label_ << "]";

    if (full) {
        descr << " ";
        descr << "source [" << sourceUri_ << "]";
        descr << " ";
        descr << "secure " << (secure_ ? "[YES]" : "[NO]");
    }

    return descr.str();
}

bool
MediaAttribute::hasValidVideo()
{
    return type_ == MediaType::MEDIA_VIDEO && enabled_&& !muted_ && !onHold_;
}
} // namespace jami
