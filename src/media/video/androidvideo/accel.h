/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

namespace ring { namespace video {

AVCodec* findAcceleratedAndroidDecoder(enum AVCodecID codec_id) {
    const char* codec_name = nullptr;
    switch (codec_id) {
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
    }
    AVCodec* codec;
    if (codec_name)
        codec = avcodec_find_decoder_by_name(codec_name);
    if (not codec)
        codec = avcodec_find_decoder(codec_id);
    return codec;
}

}} // namespace ring::video
