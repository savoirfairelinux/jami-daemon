/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
 *  Author: Eloi BAIL <eloi.bail@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef __MEDIA_CODEC_H__
#define __MEDIA_CODEC_H__

#include "libav_deps.h"
#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>

namespace ring {
    enum CODEC_TYPE : uint16_t {
        CODEC_TYPE_UNDEFINED = 0,
        CODEC_TYPE_ENCODER = 1,
        CODEC_TYPE_DECODER = 2,
        CODEC_TYPE_ENCODER_DECODER = CODEC_TYPE_ENCODER | CODEC_TYPE_DECODER
    };
    enum MEDIA_TYPE {
        MEDIA_AUDIO = 0x1,
        MEDIA_VIDEO = 0x2,
        MEDIA_CRYPTO = 0x4,
        MEDIA_ALL = MEDIA_AUDIO | MEDIA_VIDEO | MEDIA_CRYPTO
    };
    class MediaCodec{

public:
        MediaCodec(AVCodecID  avcodecId, const std::string name, std::string libName, MEDIA_TYPE mediaType, CODEC_TYPE codecType = CODEC_TYPE_UNDEFINED, uint16_t bitrate = 0, uint16_t payloadType = 0, bool isActive = true);
        virtual ~MediaCodec();

        uint16_t codecId_;
        AVCodecID  avcodecId_;
        std::string name_;
        std::string libName_;
        uint16_t payloadType_;
        bool isActive_;
        uint16_t bitrate_;
        CODEC_TYPE codecType_;
        MEDIA_TYPE mediaType_;
        uint16_t order_;

        uint16_t getCodecId();
        std::string to_string();
private:
    };
static uint16_t generateId();
static uint16_t s_codecId = 0;
bool operator== (MediaCodec codec1, MediaCodec codec2);
}
#endif // __MEDIA_CODEC_H__
