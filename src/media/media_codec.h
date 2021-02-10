/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#include "audio/audiobuffer.h" // for AudioFormat
#include "ip_utils.h"

#include <cctype>
#include <string>
#include <vector>
#include <map>
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
                    const std::string& name,
                    const std::string& libName,
                    MediaType mediaType,
                    CodecType codecType = CODEC_NONE,
                    unsigned bitrate = 0,
                    unsigned payloadType = 0,
                    unsigned m_minQuality = DEFAULT_NO_QUALITY,
                    unsigned m_maxQuality = DEFAULT_NO_QUALITY);

    virtual ~SystemCodecInfo();

    /* generic codec information */
    unsigned id;        /* id of the codec used with dbus */
    unsigned avcodecId; /* read as AVCodecID libav codec identifier */
    std::string name;
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
                         const std::string& name,
                         const std::string& libName,
                         CodecType type,
                         unsigned bitrate = 0,
                         unsigned sampleRate = 0,
                         unsigned nbChannels = 0,
                         unsigned payloadType = 0);

    ~SystemAudioCodecInfo();

    std::map<std::string, std::string> getCodecSpecifications();

    AudioFormat audioformat {AudioFormat::NONE()};
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

    std::map<std::string, std::string> getCodecSpecifications();

    unsigned frameRate;
    unsigned profileId;
    std::string parameters;
};

/*
 * AccountCodecInfo
 * represent information of a codec on a account
 * store account codec values
 */
struct AccountCodecInfo
{
    AccountCodecInfo(const SystemCodecInfo& sysCodecInfo) noexcept;
    AccountCodecInfo(const AccountCodecInfo&) noexcept = default;
    AccountCodecInfo(AccountCodecInfo&&) noexcept = delete;
    AccountCodecInfo& operator=(const AccountCodecInfo&);
    AccountCodecInfo& operator=(AccountCodecInfo&&) noexcept = delete;

    const SystemCodecInfo& systemCodecInfo;
    unsigned order {0}; /*used to define preferred codec list order in UI*/
    bool isActive {false};
    /* account custom values */
    unsigned payloadType;
    unsigned bitrate;
    unsigned quality;
    std::map<std::string, std::string> getCodecSpecifications();
};

struct AccountAudioCodecInfo : AccountCodecInfo
{
    AccountAudioCodecInfo(const SystemAudioCodecInfo& sysCodecInfo);

    std::map<std::string, std::string> getCodecSpecifications();
    void setCodecSpecifications(const std::map<std::string, std::string>& details);

    /* account custom values */
    AudioFormat audioformat {AudioFormat::NONE()};
    bool isPCMG722() const;
};

struct AccountVideoCodecInfo : AccountCodecInfo
{
    AccountVideoCodecInfo(const SystemVideoCodecInfo& sysCodecInfo);

    void setCodecSpecifications(const std::map<std::string, std::string>& details);
    std::map<std::string, std::string> getCodecSpecifications();

    /* account custom values */
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
    IpAddr addr {};

    /** RTCP socket address */
    IpAddr rtcp_addr {};

    /** RTP */
    std::shared_ptr<AccountCodecInfo> codec {};
    unsigned payload_type {};
    std::string receiving_sdp {};
    unsigned bitrate {};
    unsigned rtp_clockrate {8000};

    /** Audio parameters */
    unsigned frame_size {};

    /** Video parameters */
    std::string parameters {};
    RateMode mode {RateMode::CRF_CONSTRAINED};
    bool linkableHW {false};

    /** Crypto parameters */
    CryptoAttribute crypto {};
};

// TODO. Move to a separarate file
class MediaAttribute
{
public:
    MediaAttribute(MediaType type = MediaType::MEDIA_NONE,
                   bool muted = false,
                   bool secure = true,
                   bool enabled = true,
                   std::string_view source = {},
                   std::string_view label = {})
        : type_(type)
        , muted_(muted)
        , secure_(secure)
        , enabled_(enabled)
        , sourceUri_(source)
        , label_(label)
    {}

    MediaAttribute(const MediaAttribute& other)
        : type_(other.type_)
        , muted_(other.muted_)
        , secure_(other.secure_)
        , enabled_(other.enabled_)
        , sourceUri_(other.sourceUri_)
        , label_(other.label_) {};

    virtual ~MediaAttribute() {};

    // Media list parser
    static std::vector<MediaAttribute> parseMediaList(const std::vector<DRing::MediaMap>& mediaList);
    static MediaType stringToMediaType(const std::string& mediaType);

    static std::pair<bool, MediaType> getMediaType(const std::vector<DRing::MediaMap>& mediaList,
                                                   unsigned index);
    static std::pair<bool, bool> getBoolValue(const std::vector<DRing::MediaMap>& mediaList,
                                              unsigned index,
                                              const std::string& key);
    static std::pair<bool, std::string> getStringValue(const std::vector<DRing::MediaMap>& mediaList,
                                                       unsigned index,
                                                       const std::string& key);

    // Return true if at least one media has a matching type.
    static bool hasMediaType(const std::vector<MediaAttribute>& mediaList, MediaType type);

    // Return a string of a boolean
    static char const* boolToString(bool val);

    // Return a string of the media type
    static char const* mediaTypeToString(MediaType type);

    // Serialize a vector of MediaAttribute to a vector of MediaMap
    static std::vector<DRing::MediaMap> mediaAttributeToMediaMap(
        std::vector<MediaAttribute> mediaAttrList);

    void updateFrom(const MediaAttribute& src);

    MediaType type_;
    bool muted_;
    bool secure_;
    bool enabled_;
    std::string sourceUri_;
    std::string label_;
};

namespace MediaAttributeKey {
constexpr static char MEDIA_TYPE[] = "MEDIA_TYPE"; // string
constexpr static char ENABLED[] = "ENABLED";       // bool
constexpr static char MUTED[] = "MUTED";           // bool
constexpr static char SECURE[] = "SECURE";         // bool
constexpr static char SOURCE[] = "SOURCE";         // string
constexpr static char LABEL[] = "LABEL";           // string
} // namespace MediaAttributeKey

namespace MediaAttributeValue {
constexpr static auto TRUE_VAL = TRUE_STR;
constexpr static auto FALSE_VAL = FALSE_STR;
constexpr static auto AUDIO = "MEDIA_TYPE_AUDIO";
constexpr static auto VIDEO = "MEDIA_TYPE_VIDEO";
} // namespace MediaAttributeValue

namespace MediaStateChangedEvent {
constexpr static auto MEDIA_NEGOTIATED = "MEDIA_NEGOTIATED";
constexpr static auto MEDIA_STARTED = "MEDIA_STARTED";
constexpr static auto MEDIA_STOPPED = "MEDIA_STOPPED";
} // namespace MediaStateChangedEvent

} // namespace jami
