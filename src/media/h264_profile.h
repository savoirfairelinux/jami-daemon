/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace jami {

/**
 * H.264 profile negotiation helpers (RFC 6184).
 *
 * Profiles are expressed as FFmpeg AV_PROFILE_H264_* values, possibly
 * OR-ed with AV_PROFILE_H264_CONSTRAINED or AV_PROFILE_H264_INTRA
 * constraint flags.
 */
namespace h264 {

struct ProfileLevel
{
    int profile; ///< AV_PROFILE_H264_* value
    int level;   ///< level_idc (e.g. 0x1f for level 3.1)
};

/**
 * Parse the profile-level-id parameter from SDP fmtp parameters (RFC 6184 §8.1).
 * @return std::nullopt when absent, malformed or referencing an unknown profile.
 */
std::optional<ProfileLevel> parseProfileLevelId(std::string_view fmtpParams);

/**
 * Human-readable name of the profile signaled by fmtp parameters
 * (e.g. "High 4:4:4 Predictive"), for display in the advanced call
 * information. An absent profile-level-id implies Constrained Baseline.
 */
std::string profileName(std::string_view fmtpParams);

/**
 * Build a "profile-level-id=XXXXXX" fmtp parameter for the given
 * AV_PROFILE_H264_* profile and level_idc.
 */
std::string makeProfileLevelId(int profile, int level);

/**
 * Whether the fmtp parameters signal level-asymmetry-allowed=1 (RFC 6184 §8.1).
 */
bool levelAsymmetryAllowed(std::string_view fmtpParams);

/**
 * Rewrite the level part of the profile-level-id parameter.
 * Returns the parameters unchanged when no profile-level-id is present.
 */
std::string setLevel(std::string_view fmtpParams, int level);

/**
 * Chroma sampling (pixel format) mandated by an H.264 profile.
 */
AVPixelFormat pixelFormat(int profile);

/**
 * Whether a local encoder can produce the given profile.
 */
bool canEncode(int profile);

/**
 * Whether a local decoder can consume the given profile.
 */
bool canDecode(int profile);

/**
 * High-chroma profiles that can be both encoded and decoded locally,
 * hence offered as additional payload types (RFC 6184 §8.2.2 requires
 * symmetric profile support per payload type). Best chroma first.
 */
std::vector<int> negotiableHighProfiles();

} // namespace h264
} // namespace jami
