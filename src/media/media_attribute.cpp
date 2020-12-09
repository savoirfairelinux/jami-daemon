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
    std::vector<MediaAttribute> mediaAttrList(mediaList.size());
    for (auto const& mediaMap : mediaList) {
        mediaAttrList.emplace_back(mediaMapToMediaAttribute(mediaMap));
    }

    return mediaAttrList;
}

MediaAttribute
MediaAttribute::mediaMapToMediaAttribute(const DRing::MediaMap& mediaMap)
{
    MediaAttribute mediaAttr;
    std::pair<bool, MediaType> pairType = getMediaType(mediaMap);
    if (pairType.first)
        mediaAttr.type_ = pairType.second;

    std::pair<bool, bool> pairBool;

    pairBool = getBoolValue(mediaMap, MediaAttributeKey::MUTED);
    if (pairBool.first)
        mediaAttr.muted_ = pairBool.second;

    pairBool = getBoolValue(mediaMap, MediaAttributeKey::ENABLED);
    if (pairBool.first)
        mediaAttr.enabled_ = pairBool.second;

    std::pair<bool, std::string> pairString;
    pairString = getStringValue(mediaMap, MediaAttributeKey::SOURCE);
    if (pairBool.first)
        mediaAttr.sourceUri_ = pairString.second;

    pairString = getStringValue(mediaMap, MediaAttributeKey::LABEL);
    if (pairBool.first)
        mediaAttr.label_ = pairString.second;

    return mediaAttr;
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
MediaAttribute::getMediaType(const DRing::MediaMap& map)
{
    const auto& iter = map.find(MediaAttributeKey::MEDIA_TYPE);
    if (iter == map.end()) {
        JAMI_WARN("[MEDIA_TYPE] key not found in media map");
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
MediaAttribute::getBoolValue(const DRing::MediaMap& map, const std::string& key)
{
    const auto& iter = map.find(key);
    if (iter == map.end()) {
        JAMI_WARN("[%s] key not found for media", key.c_str());
        return {false, false};
    }

    auto const& value = iter->second;
    if (value.compare(MediaAttributeValue::TRUE_VAL) == 0)
        return {true, true};
    if (value.compare(MediaAttributeValue::FALSE_VAL) == 0)
        return {true, false};

    JAMI_ERR("Invalid value %s for a boolean key", value.c_str());
    return {false, false};
}

std::pair<bool, std::string>
MediaAttribute::getStringValue(const DRing::MediaMap& map, const std::string& key)
{
    const auto& iter = map.find(key);
    if (iter == map.end()) {
        JAMI_WARN("[%s] key not found in media map", key.c_str());
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
MediaAttribute::mediaAttributesToMediaMaps(std::vector<MediaAttribute> mediaAttrList)
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
    JAMI_DBG("Update from [%p] to [%p]", &src, this);

    type_ = src.type_;
    muted_ = src.muted_;
    secure_ = src.secure_;
    enabled_ = src.enabled_;
    sourceUri_ = src.sourceUri_;
    label_ = src.label_;
}

std::string
MediaAttribute::toString(bool full) const
{
    std::ostringstream descr;
    descr << "[" << this << "] ";
    descr << "type " << (type_ == MediaType::MEDIA_AUDIO ? "[AUDIO]" : "[VIDEO]");
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
} // namespace jami
