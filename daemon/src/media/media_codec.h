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

/*
 * SystemCodecInfo
 * represent information of a codec available on the system (using libav)
 * store default codec values
 */
struct SystemCodecInfo
{
    SystemCodecInfo(unsigned avcodecId, const std::string name,
                    std::string libName, MediaType mediaType,
                    CodecType codecType = CODEC_UNDEFINED, unsigned bitrate = 0,
                    unsigned payloadType = 0);

    virtual ~SystemCodecInfo();

    /* generic codec information */
    unsigned id_; /* id of the codec used with dbus */
    unsigned  avcodecId_;  /* read as AVCodecID libav codec identifier */
    std::string name_;
    std::string libName_;
    CodecType codecType_;
    MediaType mediaType_;

    /* default codec values */
    unsigned payloadType_;
    unsigned bitrate_;

    std::string to_string() const;
};

/*
 * SystemAudioCodecInfo
 * represent information of a audio codec available on the system (using libav)
 * store default codec values
 */
struct SystemAudioCodecInfo : SystemCodecInfo
{
    SystemAudioCodecInfo(unsigned avcodecId, const std::string name,
                         std::string libName, CodecType type,
                         unsigned bitrate = 0,
                         unsigned sampleRate = 0, unsigned nbChannels = 0,
                         unsigned payloadType = 0);

    ~SystemAudioCodecInfo();

    std::vector<std::string> getCodecSpecifications();
    bool isPCMG722() const;

    unsigned sampleRate_;
    unsigned nbChannels_;
};

/*
 * SystemVideoCodecInfo
 * represent information of a video codec available on the system (using libav)
 * store default codec values
 */
struct SystemVideoCodecInfo : SystemCodecInfo
{
    SystemVideoCodecInfo(unsigned avcodecId, const std::string name,
                         std::string libName, CodecType type = CODEC_UNDEFINED,
                         unsigned payloadType = 0);

    ~SystemVideoCodecInfo();

    std::vector<std::string> getCodecSpecifications();

    unsigned frameRate_;
    unsigned profileId_;
    std::string parameters_;
};

/*
 * AccountCodecInfo
 * represent information of a codec on a account
 * store account codec values
 */
struct AccountCodecInfo
{
    AccountCodecInfo(const SystemCodecInfo& sysCodecInfo);
    ~AccountCodecInfo();

    const SystemCodecInfo& systemCodecInfo;
    unsigned order_; /*used to define prefered codec list order in UI*/
    bool isActive_;
    /* account custom values */
    unsigned payloadType_;
    unsigned bitrate_;
};

struct AccountAudioCodecInfo : AccountCodecInfo
{
    AccountAudioCodecInfo(const SystemAudioCodecInfo& sysCodecInfo);
    ~AccountAudioCodecInfo();

    std::vector<std::string> getCodecSpecifications();

    /* account custom values */
    unsigned sampleRate_;
    unsigned nbChannels_;
};

struct AccountVideoCodecInfo : AccountCodecInfo
{
    AccountVideoCodecInfo(const SystemVideoCodecInfo& sysCodecInfo);
    ~AccountVideoCodecInfo();

    std::vector<std::string> getCodecSpecifications();

    /* account custom values */
    unsigned frameRate_;
    unsigned profileId_;
    std::string parameters_;
};
bool operator== (SystemCodecInfo codec1, SystemCodecInfo codec2);

class CryptoAttribute {
public:
    CryptoAttribute() {}
    CryptoAttribute(const std::string &tag,
                    const std::string &cryptoSuite,
                    const std::string &srtpKeyMethod,
                    const std::string &srtpKeyInfo,
                    const std::string &lifetime,
                    const std::string &mkiValue,
                    const std::string &mkiLength) :
        tag_(tag),
        cryptoSuite_(cryptoSuite),
        srtpKeyMethod_(srtpKeyMethod),
        srtpKeyInfo_(srtpKeyInfo),
        lifetime_(lifetime),
        mkiValue_(mkiValue),
        mkiLength_(mkiLength) {}

    std::string getTag() const {
        return tag_;
    }
    std::string getCryptoSuite() const {
        return cryptoSuite_;
    }
    std::string getSrtpKeyMethod() const {
        return srtpKeyMethod_;
    }
    std::string getSrtpKeyInfo() const {
        return srtpKeyInfo_;
    }
    std::string getLifetime() const {
        return lifetime_;
    }
    std::string getMkiValue() const {
        return mkiValue_;
    }
    std::string getMkiLength() const {
        return mkiLength_;
    }

    operator bool() const {
        return not tag_.empty();
    }

    std::string to_string() const {
        return tag_+" "+cryptoSuite_+" "+srtpKeyMethod_+":"+srtpKeyInfo_;
    }

private:
    std::string tag_;
    std::string cryptoSuite_;
    std::string srtpKeyMethod_;
    std::string srtpKeyInfo_;
    std::string lifetime_;
    std::string mkiValue_;
    std::string mkiLength_;
};

struct MediaDescription {
    MediaType type {};
    bool enabled {false};
    bool holding {false};
    IpAddr addr {};

    std::shared_ptr<SystemCodecInfo> codec {};
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

}//namespace ring
#endif // __MEDIA_CODEC_H__
