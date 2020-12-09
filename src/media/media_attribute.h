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
#include <config.h>
#endif

#include "media/media_codec.h"

namespace jami {

class MediaAttribute
{
public:
    MediaAttribute(MediaType type = MediaType::MEDIA_NONE,
                   bool muted = false,
                   bool secure = true,
                   bool enabled = true,
                   std::string_view source = {},
                   std::string_view label = {})
        : type_(type)
        , muted_(muted)
        , secure_(secure)
        , enabled_(enabled)
        , sourceUri_(source)
        , label_(label)
    {}

    MediaAttribute(const MediaAttribute& other)
        : type_(other.type_)
        , muted_(other.muted_)
        , secure_(other.secure_)
        , enabled_(other.enabled_)
        , sourceUri_(other.sourceUri_)
        , label_(other.label_) {};

    virtual ~MediaAttribute() {};

    // Media list parser
    static std::vector<MediaAttribute> parseMediaList(const std::vector<DRing::MediaMap>& mediaList);
    static MediaType stringToMediaType(const std::string& mediaType);

    static std::pair<bool, MediaType> getMediaType(const std::vector<DRing::MediaMap>& mediaList,
                                                   unsigned index);
    static std::pair<bool, bool> getBoolValue(const std::vector<DRing::MediaMap>& mediaList,
                                              unsigned index,
                                              const std::string& key);
    static std::pair<bool, std::string> getStringValue(const std::vector<DRing::MediaMap>& mediaList,
                                                       unsigned index,
                                                       const std::string& key);

    // Return true if at least one media has a matching type.
    static bool hasMediaType(const std::vector<MediaAttribute>& mediaList, MediaType type);

    // Return a string of a boolean
    static char const* boolToString(bool val);

    // Return a string of the media type
    static char const* mediaTypeToString(MediaType type);

    // Serialize a vector of MediaAttribute to a vector of MediaMap
    static std::vector<DRing::MediaMap> mediaAttributeToMediaMap(
        std::vector<MediaAttribute> mediaAttrList);

    void updateFrom(const MediaAttribute& src);

    MediaType type_;
    bool muted_;
    bool secure_;
    bool enabled_;
    std::string sourceUri_;
    std::string label_;
};

namespace MediaAttributeKey {
constexpr static char MEDIA_TYPE[] = "MEDIA_TYPE"; // string
constexpr static char ENABLED[] = "ENABLED";       // bool
constexpr static char MUTED[] = "MUTED";           // bool
constexpr static char SECURE[] = "SECURE";         // bool
constexpr static char SOURCE[] = "SOURCE";         // string
constexpr static char LABEL[] = "LABEL";           // string
} // namespace MediaAttributeKey

namespace MediaAttributeValue {
constexpr static auto TRUE_VAL = TRUE_STR;
constexpr static auto FALSE_VAL = FALSE_STR;
constexpr static auto AUDIO = "MEDIA_TYPE_AUDIO";
constexpr static auto VIDEO = "MEDIA_TYPE_VIDEO";
} // namespace MediaAttributeValue

namespace MediaStateChangedEvent {
constexpr static auto MEDIA_NEGOTIATED = "MEDIA_NEGOTIATED";
constexpr static auto MEDIA_STARTED = "MEDIA_STARTED";
constexpr static auto MEDIA_STOPPED = "MEDIA_STOPPED";
} // namespace MediaStateChangedEvent

} // namespace jami
