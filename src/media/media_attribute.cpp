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

namespace jami {

std::vector<MediaAttribute>
MediaAttribute::parseMediaList(const std::vector<DRing::MediaMap>& mediaList)
{
    std::vector<MediaAttribute> mediaAttrList;

    for (unsigned index = 0; index < mediaList.size(); index++) {
        MediaAttribute mediaAttr;
        std::pair<bool, MediaType> pairType = getMediaType(mediaList, index);
        if (pairType.first)
            mediaAttr.type_ = pairType.second;

        std::pair<bool, bool> pairBool;

        pairBool = getBoolValue(mediaList, index, MediaAttributeKey::MUTED);
        if (pairBool.first)
            mediaAttr.muted_ = pairBool.second;

        pairBool = getBoolValue(mediaList, index, MediaAttributeKey::ENABLED);
        if (pairBool.first)
            mediaAttr.enabled_ = pairBool.second;

        std::pair<bool, std::string> pairString;
        pairString = getStringValue(mediaList, index, MediaAttributeKey::SOURCE);
        if (pairBool.first)
            mediaAttr.sourceUri_ = pairString.second;

        pairString = getStringValue(mediaList, index, MediaAttributeKey::LABEL);
        if (pairBool.first)
            mediaAttr.label_ = pairString.second;

        mediaAttrList.emplace_back(mediaAttr);
    }

    return mediaAttrList;
}

MediaType
MediaAttribute::stringToMediaType(const std::string& mediaType)
{
    if (mediaType.compare(MediaAttributeValue::AUDIO) == 0)
        return MediaType::MEDIA_AUDIO;
    if (mediaType.compare(MediaAttributeValue::VIDEO) == 0)
        return MediaType::MEDIA_VIDEO;
    return MediaType::MEDIA_NONE;
}

std::pair<bool, MediaType>
MediaAttribute::getMediaType(const std::vector<DRing::MediaMap>& mediaList, unsigned index)
{
    assert(index < mediaList.size());
    auto map = mediaList[index];

    const auto& iter = map.find(MediaAttributeKey::MEDIA_TYPE);
    if (iter == map.end()) {
        JAMI_WARN("[MEDIA_TYPE] key not found for media @ index %u", index);
        return {false, MediaType::MEDIA_NONE};
    }

    auto type = stringToMediaType(iter->second);
    if (type == MediaType::MEDIA_NONE) {
        JAMI_ERR("Invalid value [%s] for a media type key for media @ index %u",
                 iter->second.c_str(),
                 index);
        return {false, type};
    }

    return {true, type};
}

std::pair<bool, bool>
MediaAttribute::getBoolValue(const std::vector<DRing::MediaMap>& mediaList,
                             unsigned index,
                             const std::string& key)
{
    assert(index < mediaList.size());
    auto map = mediaList[index];

    const auto& iter = map.find(key);
    if (iter == map.end()) {
        JAMI_WARN("[%s] key not found for media @ index %u", key.c_str(), index);
        return {false, false};
    }
    auto const& value = iter->second;
    if (value.compare(MediaAttributeValue::TRUE_VAL) == 0)
        return {true, true};
    if (value.compare(MediaAttributeValue::FALSE_VAL) == 0)
        return {true, false};

    JAMI_ERR("Invalid value %s for a boolean key for media @ index %u", value.c_str(), index);
    return {false, false};
}

std::pair<bool, std::string>
MediaAttribute::getStringValue(const std::vector<DRing::MediaMap>& mediaList,
                               unsigned index,
                               const std::string& key)
{
    assert(index < mediaList.size());
    auto map = mediaList[index];

    const auto& iter = map.find(key);
    if (iter == map.end()) {
        JAMI_WARN("[%s] key not found for media @ index %u", key.c_str(), index);
        return {false, {}};
    }

    return {true, iter->second};
}

char const*
MediaAttribute::boolToString(bool val)
{
    return val ? MediaAttributeValue::TRUE_VAL : MediaAttributeValue::FALSE_VAL;
}

char const*
MediaAttribute::mediaTypeToString(MediaType type)
{
    if (type == MediaType::MEDIA_AUDIO)
        return MediaAttributeValue::AUDIO;
    if (type == MediaType::MEDIA_VIDEO)
        return MediaAttributeValue::VIDEO;
    return nullptr;
}

bool
MediaAttribute::hasMediaType(const std::vector<MediaAttribute>& mediaList, MediaType type)
{
    for (auto const& media : mediaList) {
        if (media.type_ == type) {
            return true;
        }
    }
    return false;
}

std::vector<DRing::MediaMap>
MediaAttribute::mediaAttributeToMediaMap(std::vector<MediaAttribute> mediaAttrList)
{
    std::vector<DRing::MediaMap> mediaList;
    for (auto const& media : mediaAttrList) {
        mediaList.emplace_back(DRing::MediaMap {});
        DRing::MediaMap& mediaMap = mediaList.back();

        mediaMap.emplace(MediaAttributeKey::MEDIA_TYPE, mediaTypeToString(media.type_));
        mediaMap.emplace(MediaAttributeKey::LABEL, media.label_);
        mediaMap.emplace(MediaAttributeKey::ENABLED, boolToString(media.enabled_));
        mediaMap.emplace(MediaAttributeKey::MUTED, boolToString(media.muted_));
        mediaMap.emplace(MediaAttributeKey::SOURCE, media.sourceUri_);
    }

    return mediaList;
}

void
MediaAttribute::updateFrom(const MediaAttribute& src)
{
    type_ = src.type_;
    muted_ = src.muted_;
    secure_ = src.secure_;
    enabled_ = src.enabled_;
    sourceUri_ = src.sourceUri_;
    label_ = src.sourceUri_;
}

} // namespace jami
