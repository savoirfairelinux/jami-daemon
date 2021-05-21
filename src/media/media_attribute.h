/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
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

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "media/media_codec.h"

namespace jami {

class MediaAttribute
{
public:
    MediaAttribute(MediaType type = MediaType::MEDIA_NONE,
                   bool muted = false,
                   bool secure = true,
                   bool enabled = false,
                   std::string_view source = {},
                   std::string_view label = {},
                   bool onHold = false)
        : type_(type)
        , muted_(muted)
        , secure_(secure)
        , enabled_(enabled)
        , sourceUri_(source)
        , label_(label)
        , onHold_(onHold)

    {}

    MediaAttribute(const DRing::MediaMap& mediaMap);

    static std::vector<MediaAttribute> parseMediaList(const std::vector<DRing::MediaMap>& mediaList);

    static MediaType stringToMediaType(const std::string& mediaType);

    static std::pair<bool, MediaType> getMediaType(const DRing::MediaMap& map);

    static std::pair<bool, bool> getBoolValue(const DRing::MediaMap& mediaMap,
                                              const std::string& key);

    static std::pair<bool, std::string> getStringValue(const DRing::MediaMap& mediaMap,
                                                       const std::string& key);

    // Return true if at least one media has a matching type.
    static bool hasMediaType(const std::vector<MediaAttribute>& mediaList, MediaType type);

    // Return a string of a boolean
    static char const* boolToString(bool val);

    // Return a string of the media type
    static char const* mediaTypeToString(MediaType type);

    // Convert MediaAttribute to MediaMap
    static DRing::MediaMap toMediaMap(const MediaAttribute& mediaAttr);

    // Serialize a vector of MediaAttribute to a vector of MediaMap
    static std::vector<DRing::MediaMap> mediaAttributesToMediaMaps(
        std::vector<MediaAttribute> mediaAttrList);

    std::string toString(bool full = false) const;

    MediaType type_ {MediaType::MEDIA_NONE};
    bool muted_ {false};
    bool secure_ {true};
    bool enabled_ {false};
    std::string sourceUri_ {};
    std::string label_ {};
    bool onHold_ {false};

    // NOTE: the hold and mute attributes are related but not
    // tightly coupled. A hold/un-hold operation should always
    // trigger a new re-invite to change the change the media
    // attributes. For instance, on an active call, the hold
    // action would change media direction from "sendrecv" to
    // "sendonly".
    // In contrast, the mute attribute describe the presence
    // (or absence) of media signal in the stream. In other
    // words, the mute action can be performed without requiring
    // a media direction change. For instance, muting the audio
    // can be done by disabling the audio input (capture) of the
    // encoding session, resulting in sending an RTP stream without
    // actual audio (silence).
};

namespace MediaAttributeKey {
constexpr static char MEDIA_TYPE[] = "MEDIA_TYPE"; // string
constexpr static char ENABLED[] = "ENABLED";       // bool
constexpr static char MUTED[] = "MUTED";           // bool
constexpr static char SOURCE[] = "SOURCE";         // string
constexpr static char LABEL[] = "LABEL";           // string
constexpr static char ON_HOLD[] = "ON_HOLD";       // bool
} // namespace MediaAttributeKey

namespace MediaAttributeValue {
constexpr static auto AUDIO = "MEDIA_TYPE_AUDIO";
constexpr static auto VIDEO = "MEDIA_TYPE_VIDEO";
} // namespace MediaAttributeValue

namespace MediaNegotiationStatusEvents {
constexpr static auto NEGOTIATION_SUCCESS = "NEGOTIATION_SUCCESS";
constexpr static auto NEGOTIATION_FAIL = "NEGOTIATION_FAIL";
} // namespace MediaNegotiationStatusEvents

} // namespace jami
