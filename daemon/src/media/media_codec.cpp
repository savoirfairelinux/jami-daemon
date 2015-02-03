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
#include "media_codec.h"
#include <string.h>

namespace ring {
MediaCodec::MediaCodec(/*AVCodecID*/ uint16_t avcodecId, const std::string name, std::string libName, MEDIA_TYPE mediaType, CODEC_TYPE codecType, uint16_t bitrate, uint16_t payloadType, bool isActive)
{
    avcodecId_ = avcodecId;
    name_ = name;
    payloadType_ = payloadType;
    isActive_ = isActive;
    libName_ = libName;
    bitrate_ = bitrate;
    codecId_ = generateId();
    codecType_ = codecType;
    mediaType_ = mediaType;
}
MediaCodec::~MediaCodec()
{
    //TODO
}
uint16_t MediaCodec::getCodecId()
{
    return codecId_;
}
std::string MediaCodec::to_string()
{
    uint16_t len =
        strlen("codecid=") +
        sizeof(uint16_t) +
        strlen("avcodecid=") +
        sizeof(uint16_t) +
        strlen("name=") +
        strlen(name_.c_str()) +
        strlen("PT=") +
        sizeof(uint16_t) +
        strlen("isActive") +
        strlen("false") + // worth case strlen(false) > strlen(true)
        strlen("libName") +
        strlen(libName_.c_str()) +
        strlen("bitrate=") +
        sizeof(uint16_t) +
        1;


    char* ret = (char*) malloc(len);
    snprintf(ret, len, "type %d: codecid=%d,avcodecid=%d,name=%s,PT=%d,isActive=%s,libName=%s,bitrate=%d",
            codecType_,
            codecId_, avcodecId_, name_.c_str(), payloadType_, (isActive_ ? "true " : "false"), libName_.c_str(), bitrate_);

    std::string retString (ret);
    free(ret);
    return retString;
}
static uint16_t generateId()
{
    return s_codecId++;
}

bool operator== (MediaCodec codec1, MediaCodec codec2)
{
    //return ((codec1.avcodecId_ == codec2.avcodecId_) && (codec1.name_.compare(codec2.name_) == 0));
    return (codec1.avcodecId_ == codec2.avcodecId_);
}
}
