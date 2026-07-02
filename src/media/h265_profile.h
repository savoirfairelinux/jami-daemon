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

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace jami {

/**
 * H.265/HEVC profile negotiation helpers (RFC 7798).
 *
 * HEVC profiles are signaled in SDP by the profile-space, profile-id,
 * tier-flag and interop-constraints parameters (RFC 7798 §7.1). The
 * high chroma profiles (Main 4:4:4, Main 4:2:2 10) belong to the Format
 * Range Extensions (profile-id 4) and are differentiated by constraint
 * flags carried in interop-constraints.
 */
namespace h265 {

enum class Profile {
    Main,      ///< profile-id 1, 4:2:0 8-bit
    Main444,   ///< profile-id 4, Main 4:4:4 (RExt)
    Main422_10 ///< profile-id 4, Main 4:2:2 10 (RExt)
};

/**
 * H.265 payload format parameters relevant to profile negotiation,
 * with the default values mandated by RFC 7798 §7.1 when absent.
 */
struct FmtpInfo
{
    int profileSpace {0};
    int profileId {1}; ///< Main profile
    int tierFlag {0};
    int levelId {93};                             ///< level 3.1
    uint64_t interopConstraints {0xB00000000000}; ///< 48 bits
};

/**
 * Parse H.265 fmtp parameters (semicolon-separated name=value pairs,
 * optionally prefixed by the payload type). Unknown parameters are
 * ignored; absent parameters take their RFC 7798 defaults.
 */
FmtpInfo parseFmtp(std::string_view fmtpParams);

/**
 * Identify the negotiable profile signaled by fmtp parameters.
 * @return std::nullopt for profiles this implementation cannot
 * negotiate symmetrically (RFC 7798 §7.2.2).
 */
std::optional<Profile> profileFromFmtp(const FmtpInfo& info);

/**
 * Build the fmtp parameters advertising a profile at the given level-id.
 */
std::string fmtpParams(Profile profile, int levelId);

/**
 * Chroma sampling (pixel format) used to encode a profile.
 */
AVPixelFormat pixelFormat(Profile profile);

/**
 * Whether a local encoder can produce the given profile.
 */
bool canEncode(Profile profile);

/**
 * Whether a local decoder can consume the given profile.
 */
bool canDecode(Profile profile);

/**
 * High chroma profiles that can be both encoded and decoded locally,
 * hence offered as additional payload types (RFC 7798 §7.2.2 requires
 * symmetric profile support per payload type). Best chroma first.
 */
std::vector<Profile> negotiableHighProfiles();

/**
 * Rewrite (or append) the level-id parameter.
 */
std::string setLevelId(std::string_view fmtpParams, int levelId);

} // namespace h265
} // namespace jami
