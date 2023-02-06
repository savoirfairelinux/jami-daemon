/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Eloi Bail <eloi.bail@savoirfairelinux.com>
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
#ifndef LIBJAMI_MEDIA_H
#define LIBJAMI_MEDIA_H

#include "def.h"

namespace libjami {

namespace Media {

// Supported MRL schemes
namespace VideoProtocolPrefix {

constexpr static const char* NONE = "";
constexpr static const char* DISPLAY = "display";
constexpr static const char* FILE = "file";
constexpr static const char* CAMERA = "camera";
constexpr static const char* SEPARATOR = "://";
} // namespace VideoProtocolPrefix

namespace Details {

constexpr static char MEDIA_TYPE_AUDIO[] = "MEDIA_TYPE_AUDIO";
constexpr static char MEDIA_TYPE_VIDEO[] = "MEDIA_TYPE_VIDEO";

// Renderer and Shm info
constexpr static char CALL_ID[] = "CALL_ID";
constexpr static char SHM_PATH[] = "SHM_PATH";
constexpr static char WIDTH[] = "WIDTH";
constexpr static char HEIGHT[] = "HEIGHT";

} // namespace Details

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
constexpr static auto SRC_TYPE_NONE = "NONE";
constexpr static auto SRC_TYPE_CAPTURE_DEVICE = "CAPTURE_DEVICE";
constexpr static auto SRC_TYPE_DISPLAY = "DISPLAY";
constexpr static auto SRC_TYPE_FILE = "FILE";
} // namespace MediaAttributeValue

namespace MediaNegotiationStatusEvents {
constexpr static auto NEGOTIATION_SUCCESS = "NEGOTIATION_SUCCESS";
constexpr static auto NEGOTIATION_FAIL = "NEGOTIATION_FAIL";
} // namespace MediaNegotiationStatusEvents

} // namespace Media

} // namespace libjami

#endif
