/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "audio/audio_format.h"

#include <dhtnet/ip_utils.h>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <unistd.h>

namespace jami {

enum class KeyExchangeProtocol { NONE, SDES };

enum CodecType : unsigned {
    CODEC_NONE = 0, // indicates that no codec is used or defined
    CODEC_ENCODER = 1,
    CODEC_DECODER = 2,
    CODEC_ENCODER_DECODER = CODEC_ENCODER | CODEC_DECODER
};

enum MediaType : unsigned {
    MEDIA_NONE = 0, // indicates that no media is used or defined
    MEDIA_AUDIO = 1,
    MEDIA_VIDEO = 2,
    MEDIA_ALL = MEDIA_AUDIO | MEDIA_VIDEO
};

enum class RateMode : unsigned { CRF_CONSTRAINED, CQ, CBR };

/*
 * SystemCodecInfo
 * represent information of a codec available on the system (using libav)
 * store default codec values
 */
struct SystemCodecInfo
{
    static constexpr unsigned DEFAULT_CODEC_QUALITY {30};
#ifdef ENABLE_VIDEO
    static constexpr unsigned DEFAULT_H264_MIN_QUALITY {40};
    static constexpr unsigned DEFAULT_H264_MAX_QUALITY {20};
    static constexpr unsigned DEFAULT_VP8_MIN_QUALITY {50};
    static constexpr unsigned DEFAULT_VP8_MAX_QUALITY {20};
#endif

    // indicates that the codec does not use quality factor
    static constexpr unsigned DEFAULT_NO_QUALITY {0};

    static constexpr unsigned DEFAULT_MIN_BITRATE {200};
    static constexpr unsigned DEFAULT_MAX_BITRATE {6000};
    static constexpr unsigned DEFAULT_VIDEO_BITRATE {800}; // in Kbits/second

    SystemCodecInfo(unsigned codecId,
                    unsigned avcodecId,
                    const std::string& longName,
                    const std::string& name,
                    const std::string& libName,
                    MediaType mediaType,
                    CodecType codecType = CODEC_NONE,
                    unsigned bitrate = 0,
                    unsigned payloadType = 0,
                    unsigned m_minQuality = DEFAULT_NO_QUALITY,
                    unsigned m_maxQuality = DEFAULT_NO_QUALITY);

    virtual ~SystemCodecInfo();
    virtual std::map<std::string, std::string> getCodecSpecifications() const;

    /* generic codec information */
    unsigned id;        /* id of the codec used with dbus */
    unsigned avcodecId; /* AVCodecID libav codec identifier */
    std::string longName;   /* User-friendly codec name */
    std::string name;       /* RTP codec name as specified by http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml */
    std::string libName;
    CodecType codecType;
    MediaType mediaType;

    /* default codec values */
    unsigned payloadType;
    unsigned bitrate;
    unsigned minBitrate = DEFAULT_MIN_BITRATE;
    unsigned maxBitrate = DEFAULT_MAX_BITRATE;
    unsigned minQuality = DEFAULT_NO_QUALITY;
    unsigned maxQuality = DEFAULT_NO_QUALITY;

    // User-preferences from client
    unsigned order {0}; /*used to define preferred codec list order in UI*/
    bool isActive {false};
    unsigned quality;
};

/*
 * SystemAudioCodecInfo
 * represent information of a audio codec available on the system (using libav)
 * store default codec values
 */
struct SystemAudioCodecInfo : SystemCodecInfo
{
    SystemAudioCodecInfo(unsigned codecId,
                         unsigned avcodecId,
                         const std::string& longName,
                         const std::string& name,
                         const std::string& libName,
                         CodecType type,
                         unsigned bitrate = 0,
                         unsigned sampleRate = 0,
                         unsigned nbChannels = 0,
                         unsigned payloadType = 0,
                         AVSampleFormat sampleFormat = AV_SAMPLE_FMT_S16);

    ~SystemAudioCodecInfo();

    std::map<std::string, std::string> getCodecSpecifications() const override;
    void setCodecSpecifications(const std::map<std::string, std::string>& details);

    AudioFormat audioformat {AudioFormat::NONE()};
    bool isPCMG722() const;
};

/*
 * SystemVideoCodecInfo
 * represent information of a video codec available on the system (using libav)
 * store default codec values
 */
struct SystemVideoCodecInfo : SystemCodecInfo
{
    SystemVideoCodecInfo(unsigned codecId,
                         unsigned avcodecId,
                         const std::string& longName,
                         const std::string& name,
                         const std::string& libName,
                         CodecType type = CODEC_NONE,
                         unsigned bitrate = 0,
                         unsigned m_minQuality = 0,
                         unsigned m_maxQuality = 0,
                         unsigned payloadType = 0,
                         unsigned frameRate = 0,
                         unsigned profileId = 0);

    ~SystemVideoCodecInfo();

    void setCodecSpecifications(const std::map<std::string, std::string>& details);
    std::map<std::string, std::string> getCodecSpecifications() const override;

    unsigned frameRate;
    unsigned profileId;
    std::string parameters;
    bool isAutoQualityEnabled {true};
};
bool operator==(SystemCodecInfo codec1, SystemCodecInfo codec2);

class CryptoAttribute
{
public:
    CryptoAttribute() {}
    CryptoAttribute(const std::string& tag,
                    const std::string& cryptoSuite,
                    const std::string& srtpKeyMethod,
                    const std::string& srtpKeyInfo,
                    const std::string& lifetime,
                    const std::string& mkiValue,
                    const std::string& mkiLength)
        : tag_(tag)
        , cryptoSuite_(cryptoSuite)
        , srtpKeyMethod_(srtpKeyMethod)
        , srtpKeyInfo_(srtpKeyInfo)
        , lifetime_(lifetime)
        , mkiValue_(mkiValue)
        , mkiLength_(mkiLength)
    {}

    std::string getTag() const { return tag_; }
    std::string getCryptoSuite() const { return cryptoSuite_; }
    std::string getSrtpKeyMethod() const { return srtpKeyMethod_; }
    std::string getSrtpKeyInfo() const { return srtpKeyInfo_; }
    std::string getLifetime() const { return lifetime_; }
    std::string getMkiValue() const { return mkiValue_; }
    std::string getMkiLength() const { return mkiLength_; }

    inline explicit operator bool() const { return not tag_.empty(); }

    std::string to_string() const
    {
        return tag_ + " " + cryptoSuite_ + " " + srtpKeyMethod_ + ":" + srtpKeyInfo_;
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

// Possible values for media direction attribute. 'UNKNOWN' means that the
// direction was not set yet. Useful to detect errors when parsing the SDP.
enum class MediaDirection { SENDRECV, SENDONLY, RECVONLY, INACTIVE, UNKNOWN };

// Possible values for media transport attribute. 'UNKNOWN' means that the
// was not set, or not found when parsing. Useful to detect errors when
// parsing the SDP.
enum class MediaTransport { RTP_AVP, RTP_SAVP, UNKNOWN };

/**
 * MediaDescription
 * Negotiated RTP media slot
 */
struct MediaDescription
{
    /** Audio / video */
    MediaType type {};
    bool enabled {false};
    bool onHold {false};
    MediaDirection direction_ {MediaDirection::UNKNOWN};

    /** Endpoint socket address */
    dhtnet::IpAddr addr {};

    /** RTCP socket address */
    dhtnet::IpAddr rtcp_addr {};

    /** RTP */
    std::shared_ptr<SystemCodecInfo> codec {};
    unsigned payload_type {};
    std::string receiving_sdp {};
    unsigned bitrate {};
    unsigned rtp_clockrate {8000};

    /** Audio parameters */
    unsigned frame_size {};
    bool fecEnabled {false};

    /** Video parameters */
    std::string parameters {};
    RateMode mode {RateMode::CRF_CONSTRAINED};
    bool linkableHW {false};

    /** Crypto parameters */
    CryptoAttribute crypto {};
};
} // namespace jami
