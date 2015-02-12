/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
 *  Author: Eloi BAIL <eloi.bail@savoirfairelinux.com>
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

#include "audio/audiobuffer.h" // for AudioFormat
#include "sip/sdes_negotiator.h"
#include "ip_utils.h"

#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>

namespace ring {

enum CodecType : unsigned {
    CODEC_UNDEFINED = 0,
    CODEC_ENCODER = 1,
    CODEC_DECODER = 2,
    CODEC_ENCODER_DECODER = CODEC_ENCODER | CODEC_DECODER
};

enum MediaType : unsigned {
    MEDIA_UNDEFINED = 0,
    MEDIA_AUDIO = 1,
    MEDIA_VIDEO = 2,
    MEDIA_ALL = MEDIA_AUDIO | MEDIA_VIDEO
};

struct MediaCodec {
    MediaCodec(unsigned  avcodecId, const std::string name, std::string libName, MediaType mediaType, CodecType codecType = CODEC_UNDEFINED, uint16_t bitrate = 0, uint16_t payloadType = 0, bool isActive = true);
    virtual ~MediaCodec();

    bool isPCMG722() const;

    uint16_t codecId_;
    unsigned  avcodecId_;
    std::string name_;
    std::string libName_;
    uint16_t payloadType_;
    bool isActive_;
    uint16_t bitrate_;
    CodecType codecType_;
    MediaType mediaType_;
    uint16_t order_;

    uint16_t getCodecId();
    std::string to_string();
};
bool operator== (MediaCodec codec1, MediaCodec codec2);

struct MediaDescription {
    MediaType type {};
    bool enabled {false};
    bool holding {false};
    IpAddr addr {};

    MediaCodec* codec {};
    std::string payload_type {};
    std::string receiving_sdp {};
    unsigned bitrate {};

    //audio
    AudioFormat audioformat {AudioFormat::NONE()};
    unsigned frame_size {};

    //video
    std::string parameters {};

    //crypto
    CryptoAttribute crypto {};
};

struct DeviceParams {
    std::string input {};
    std::string format {};
    unsigned width {}, height {};
    unsigned framerate {};
    std::string video_size {};
    std::string channel {};
    std::string loop {};
    std::string sdp_flags {};
};

}
#endif // __MEDIA_CODEC_H__
