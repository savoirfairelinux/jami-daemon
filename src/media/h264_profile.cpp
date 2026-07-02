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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "h264_profile.h"

#include <fmt/format.h>

namespace jami {
namespace h264 {

std::optional<ProfileLevel>
parseProfileLevelId(std::string_view fmtpParams)
{
    static constexpr std::string_view target = "profile-level-id=";
    auto needle = fmtpParams.find(target);
    if (needle == std::string_view::npos)
        return std::nullopt;
    needle += target.size();

    static constexpr size_t idLength = 6; /* hex digits */
    if (fmtpParams.size() - needle < idLength)
        return std::nullopt;
    unsigned value = 0;
    for (size_t i = 0; i < idLength; i++) {
        const char c = fmtpParams[needle + i];
        int digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            return std::nullopt;
        value = (value << 4) | digit;
    }

    const unsigned char profile_idc = value >> 16;
    const unsigned char profile_iop = (value >> 8) & 0xff;
    ProfileLevel result;
    result.level = value & 0xff;

    switch (profile_idc) {
    case AV_PROFILE_H264_BASELINE:
        result.profile = profile_idc;
        // constraint_set1_flag
        if (profile_iop & 0x40)
            result.profile |= AV_PROFILE_H264_CONSTRAINED;
        break;
    case AV_PROFILE_H264_MAIN:
    case AV_PROFILE_H264_EXTENDED:
    case AV_PROFILE_H264_HIGH:
        result.profile = profile_idc;
        break;
    case AV_PROFILE_H264_HIGH_10:
    case AV_PROFILE_H264_HIGH_422:
    case AV_PROFILE_H264_HIGH_444_PREDICTIVE:
        result.profile = profile_idc;
        // constraint_set3_flag
        if (profile_iop & 0x10)
            result.profile |= AV_PROFILE_H264_INTRA;
        break;
    default:
        return std::nullopt;
    }
    return result;
}

std::string
makeProfileLevelId(int profile, int level)
{
    const unsigned char profile_idc = profile & ~(AV_PROFILE_H264_CONSTRAINED | AV_PROFILE_H264_INTRA);
    unsigned char profile_iop = 0;
    switch (profile_idc) {
    case AV_PROFILE_H264_BASELINE:
        // constraint_set0_flag: conforms to Baseline
        profile_iop = 0x80;
        if (profile & AV_PROFILE_H264_CONSTRAINED)
            profile_iop |= 0x40 | 0x20; // constraint_set1_flag, constraint_set2_flag
        break;
    case AV_PROFILE_H264_HIGH_10:
    case AV_PROFILE_H264_HIGH_422:
    case AV_PROFILE_H264_HIGH_444_PREDICTIVE:
        if (profile & AV_PROFILE_H264_INTRA)
            profile_iop = 0x10; // constraint_set3_flag
        break;
    default:
        break;
    }
    return fmt::format("profile-level-id={:02x}{:02x}{:02x}", profile_idc, profile_iop, level & 0xff);
}

AVPixelFormat
pixelFormat(int profile)
{
    switch (profile & ~(AV_PROFILE_H264_CONSTRAINED | AV_PROFILE_H264_INTRA)) {
    case AV_PROFILE_H264_HIGH_444_PREDICTIVE:
        return AV_PIX_FMT_YUV444P;
    case AV_PROFILE_H264_HIGH_422:
        return AV_PIX_FMT_YUV422P;
    default:
        return AV_PIX_FMT_YUV420P;
    }
}

bool
canEncode(int profile)
{
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
        return false;
    const auto format = pixelFormat(profile);
    if (format == AV_PIX_FMT_YUV420P)
        return true;
    if (!codec->pix_fmts)
        return false;
    for (const auto* p = codec->pix_fmts; *p != AV_PIX_FMT_NONE; ++p)
        if (*p == format)
            return true;
    return false;
}

bool
canDecode(int profile)
{
    // FFmpeg's software H.264 decoder supports every profile up to
    // High 4:4:4 Predictive; hardware decoders fall back to it.
    (void) profile;
    return avcodec_find_decoder(AV_CODEC_ID_H264) != nullptr;
}

std::vector<int>
negotiableHighProfiles()
{
    std::vector<int> profiles;
    for (int profile : {AV_PROFILE_H264_HIGH_444_PREDICTIVE, AV_PROFILE_H264_HIGH_422}) {
        if (canEncode(profile) && canDecode(profile))
            profiles.push_back(profile);
    }
    return profiles;
}

} // namespace h264
} // namespace jami
