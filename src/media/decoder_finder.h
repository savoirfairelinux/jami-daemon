/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace jami {

/**
 * Attempt to find standalone AVCodec decoder using AVCodecID,
 * or fallback to the default decoder.
 */
const AVCodec*
findDecoder(const enum AVCodecID codec_id)
{
    const char* codec_name;
    switch (codec_id) {
#if 0 && defined(__ANDROID__) && defined(RING_ACCEL)
    case AV_CODEC_ID_MPEG4:
        codec_name = "mpeg4_mediacodec"; break;
    case AV_CODEC_ID_H264:
        codec_name = "h264_mediacodec"; break;
    case AV_CODEC_ID_HEVC:
        codec_name = "hevc_mediacodec"; break;
    case AV_CODEC_ID_VP8:
        codec_name = "vp8_mediacodec"; break;
    case AV_CODEC_ID_VP9:
        codec_name = "vp9_mediacodec"; break;
#endif
    case AV_CODEC_ID_OPUS:
        codec_name = "libopus"; break;
    default:
        codec_name = nullptr;
    }
    const AVCodec* codec = nullptr;
    if (codec_name)
        codec = avcodec_find_decoder_by_name(codec_name);
    if (not codec)
        codec = avcodec_find_decoder(codec_id);
    return codec;
}

} // namespace jami
