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

#ifndef __MEDIA_VIDEO_CODEC_H__
#define __MEDIA_VIDEO_CODEC_H__

#include "media_codec.h"

namespace ring {
    class MediaVideoCodec: public MediaCodec {

public:
    MediaVideoCodec(AVCodecID avcodecId, const std::string name, std::string libName, CODEC_TYPE type = CODEC_TYPE_UNDEFINED, uint16_t payloadType = -1, bool isActive = true);
    ~MediaVideoCodec();

    uint16_t frameRate_;
    uint16_t profileId_;
    std::string parameters_;
    std::vector<std::string> getCodecSpecifications();

private:
    };
}
#endif //__MEDIA_VIDEO_CODEC_H__
