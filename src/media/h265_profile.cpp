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
#include "h265_profile.h"
#include "logger.h"

#include <fmt/format.h>

#include <charconv>
#include <map>
#include <mutex>

namespace jami {
namespace h265 {

// interop-constraints values (48 bits, RFC 7798 §7.1): the four source
// flags default to progressive=1, interlaced=0, non-packed=1,
// frame-only=1 (0xB); for Range Extensions profiles (profile-id 4) the
// following 9 bits carry the constraint flags differentiating the
// profile (H.265 A.3.5): max_12bit, max_10bit, max_8bit, max_422chroma,
// max_420chroma, max_monochrome, intra, one_picture_only,
// lower_bit_rate.
static constexpr uint64_t REXT_FLAGS_MASK = 0x0FF800000000; // bits 43..35
static constexpr uint64_t MAIN_444_CONSTRAINTS = 0xBE0800000000;
static constexpr uint64_t MAIN_422_10_CONSTRAINTS = 0xBD0800000000;

FmtpInfo
parseFmtp(std::string_view fmtpParams)
{
    FmtpInfo info;
    // Skip the payload type prefix of pjmedia fmtp attribute values
    if (auto space = fmtpParams.find(' '); space != std::string_view::npos && fmtpParams.find('=') > space)
        fmtpParams.remove_prefix(space + 1);

    auto parse = [](std::string_view value, auto& out, int base) {
        using T = std::decay_t<decltype(out)>;
        T result;
        auto [p, ec] = std::from_chars(value.data(), value.data() + value.size(), result, base);
        if (ec == std::errc())
            out = result;
    };

    while (!fmtpParams.empty()) {
        auto end = fmtpParams.find(';');
        auto token = fmtpParams.substr(0, end);
        fmtpParams.remove_prefix(end == std::string_view::npos ? fmtpParams.size() : end + 1);
        while (!token.empty() && token.front() == ' ')
            token.remove_prefix(1);
        auto eq = token.find('=');
        if (eq == std::string_view::npos)
            continue;
        auto name = token.substr(0, eq);
        auto value = token.substr(eq + 1);
        if (name == "profile-space")
            parse(value, info.profileSpace, 10);
        else if (name == "profile-id")
            parse(value, info.profileId, 10);
        else if (name == "tier-flag")
            parse(value, info.tierFlag, 10);
        else if (name == "level-id")
            parse(value, info.levelId, 10);
        else if (name == "interop-constraints")
            parse(value, info.interopConstraints, 16);
    }
    return info;
}

std::optional<Profile>
profileFromFmtp(const FmtpInfo& info)
{
    if (info.profileSpace != 0)
        return std::nullopt;
    switch (info.profileId) {
    case 1:
        return Profile::Main;
    case 2:
        return Profile::Main10;
    case 4:
        // Compare the Range Extensions constraint flags, ignoring the
        // source flags and the reserved bits.
        if ((info.interopConstraints & REXT_FLAGS_MASK) == (MAIN_444_CONSTRAINTS & REXT_FLAGS_MASK))
            return Profile::Main444;
        if ((info.interopConstraints & REXT_FLAGS_MASK) == (MAIN_422_10_CONSTRAINTS & REXT_FLAGS_MASK))
            return Profile::Main422_10;
        return std::nullopt;
    default:
        return std::nullopt;
    }
}

std::string
fmtpParams(Profile profile, int levelId)
{
    switch (profile) {
    case Profile::Main444:
        return fmt::format("profile-id=4;level-id={};interop-constraints={:012X}", levelId, MAIN_444_CONSTRAINTS);
    case Profile::Main422_10:
        return fmt::format("profile-id=4;level-id={};interop-constraints={:012X}", levelId, MAIN_422_10_CONSTRAINTS);
    case Profile::Main10:
        return fmt::format("profile-id=2;level-id={}", levelId);
    case Profile::Main:
    default:
        return fmt::format("profile-id=1;level-id={}", levelId);
    }
}

AVPixelFormat
pixelFormat(Profile profile)
{
    switch (profile) {
    case Profile::Main444:
        return AV_PIX_FMT_YUV444P;
    case Profile::Main422_10:
        return AV_PIX_FMT_YUV422P;
    case Profile::Main10:
        // Semi-planar 10-bit 4:2:0: the input format of hardware HEVC
        // 10-bit encoders (NVENC, VideoToolbox, QSV)
        return AV_PIX_FMT_P010;
    default:
        return AV_PIX_FMT_YUV420P;
    }
}

bool
canEncode(Profile profile)
{
    if (profile == Profile::Main)
        return avcodec_find_encoder(AV_CODEC_ID_HEVC) != nullptr;

    // No software HEVC encoder is bundled: higher chroma or bit depth
    // requires a hardware encoder (e.g. NVENC, VideoToolbox) accepting
    // the corresponding software input format. Attempt-open to probe
    // actual availability.
    const auto format = pixelFormat(profile);
    static std::mutex mutex;
    static std::map<AVPixelFormat, bool> cache;
    std::lock_guard lock(mutex);
    if (auto it = cache.find(format); it != cache.end())
        return it->second;

    bool supported = false;
    void* iter = nullptr;
    while (const AVCodec* codec = av_codec_iterate(&iter)) {
        if (codec->id != AV_CODEC_ID_HEVC || !av_codec_is_encoder(codec) || !codec->pix_fmts)
            continue;
        bool takesFormat = false;
        for (const auto* p = codec->pix_fmts; *p != AV_PIX_FMT_NONE; ++p)
            takesFormat |= (*p == format);
        if (!takesFormat)
            continue;
        AVCodecContext* ctx = avcodec_alloc_context3(codec);
        if (!ctx)
            continue;
        ctx->width = 640;
        ctx->height = 480;
        ctx->pix_fmt = format;
        ctx->time_base = {1, 30};
        ctx->framerate = {30, 1};
        if (avcodec_open2(ctx, codec, nullptr) >= 0) {
            JAMI_LOG("HEVC encoder {} supports {}", codec->name, av_get_pix_fmt_name(format));
            supported = true;
        }
        avcodec_free_context(&ctx);
        if (supported)
            break;
    }
    cache.emplace(format, supported);
    return supported;
}

bool
canDecode(Profile profile)
{
    // FFmpeg's software HEVC decoder supports the Main and Format Range
    // Extensions profiles; hardware decoders fall back to it.
    (void) profile;
    return avcodec_find_decoder(AV_CODEC_ID_HEVC) != nullptr;
}

std::vector<Profile>
negotiableHighProfiles()
{
    std::vector<Profile> profiles;
    for (auto profile : {Profile::Main444, Profile::Main422_10, Profile::Main10}) {
        if (canEncode(profile) && canDecode(profile))
            profiles.push_back(profile);
    }
    return profiles;
}

std::string
setLevelId(std::string_view fmtpParams, int levelId)
{
    std::string result;
    bool replaced = false;
    while (true) {
        auto end = fmtpParams.find(';');
        auto token = fmtpParams.substr(0, end);
        auto name = token.substr(0, token.find('='));
        if (!result.empty())
            result += ';';
        if (name == "level-id") {
            result += fmt::format("level-id={}", levelId);
            replaced = true;
        } else {
            result += token;
        }
        if (end == std::string_view::npos)
            break;
        fmtpParams.remove_prefix(end + 1);
    }
    if (!replaced) {
        if (!result.empty())
            result += ';';
        result += fmt::format("level-id={}", levelId);
    }
    return result;
}

} // namespace h265
} // namespace jami
