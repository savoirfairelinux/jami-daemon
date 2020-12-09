/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "media_codec.h"
#include "account_const.h"

#include "string_utils.h"
#include "logger.h"

#include <string>
#include <sstream>

namespace jami {

/*
 * SystemCodecInfo
 */
SystemCodecInfo::SystemCodecInfo(unsigned codecId,
                                 unsigned avcodecId,
                                 const std::string& name,
                                 const std::string& libName,
                                 MediaType mediaType,
                                 CodecType codecType,
                                 unsigned bitrate,
                                 unsigned payloadType,
                                 unsigned minQuality,
                                 unsigned maxQuality)
    : id(codecId)
    , avcodecId(avcodecId)
    , name(name)
    , libName(libName)
    , codecType(codecType)
    , mediaType(mediaType)
    , payloadType(payloadType)
    , bitrate(bitrate)
    , minQuality(minQuality)
    , maxQuality(maxQuality)
{}

SystemCodecInfo::~SystemCodecInfo() {}

/*
 * SystemAudioCodecInfo
 */
SystemAudioCodecInfo::SystemAudioCodecInfo(unsigned codecId,
                                           unsigned m_avcodecId,
                                           const std::string& m_name,
                                           const std::string& m_libName,
                                           CodecType m_type,
                                           unsigned m_bitrate,
                                           unsigned m_sampleRate,
                                           unsigned m_nbChannels,
                                           unsigned m_payloadType)
    : SystemCodecInfo(codecId,
                      m_avcodecId,
                      m_name,
                      m_libName,
                      MEDIA_AUDIO,
                      m_type,
                      m_bitrate,
                      m_payloadType)
    , audioformat {m_sampleRate, m_nbChannels}
{}

SystemAudioCodecInfo::~SystemAudioCodecInfo() {}

std::map<std::string, std::string>
SystemAudioCodecInfo::getCodecSpecifications()
{
    return {{DRing::Account::ConfProperties::CodecInfo::NAME, name},
            {DRing::Account::ConfProperties::CodecInfo::TYPE,
             (mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
            {DRing::Account::ConfProperties::CodecInfo::BITRATE, std::to_string(bitrate)},
            {DRing::Account::ConfProperties::CodecInfo::SAMPLE_RATE,
             std::to_string(audioformat.sample_rate)},
            {DRing::Account::ConfProperties::CodecInfo::CHANNEL_NUMBER,
             std::to_string(audioformat.nb_channels)}};
}

/*
 * SystemVideoCodecInfo
 */
SystemVideoCodecInfo::SystemVideoCodecInfo(unsigned codecId,
                                           unsigned m_avcodecId,
                                           const std::string& m_name,
                                           const std::string& m_libName,
                                           CodecType m_type,
                                           unsigned m_bitrate,
                                           unsigned m_minQuality,
                                           unsigned m_maxQuality,
                                           unsigned m_payloadType,
                                           unsigned m_frameRate,
                                           unsigned m_profileId)
    : SystemCodecInfo(codecId,
                      m_avcodecId,
                      m_name,
                      m_libName,
                      MEDIA_VIDEO,
                      m_type,
                      m_bitrate,
                      m_payloadType,
                      m_minQuality,
                      m_maxQuality)
    , frameRate(m_frameRate)
    , profileId(m_profileId)
{}

SystemVideoCodecInfo::~SystemVideoCodecInfo() {}

std::map<std::string, std::string>
SystemVideoCodecInfo::getCodecSpecifications()
{
    return {
        {DRing::Account::ConfProperties::CodecInfo::NAME, name},
        {DRing::Account::ConfProperties::CodecInfo::TYPE,
         (mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
        {DRing::Account::ConfProperties::CodecInfo::BITRATE, std::to_string(bitrate)},
        {DRing::Account::ConfProperties::CodecInfo::FRAME_RATE, std::to_string(frameRate)},
        {DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE, std::to_string(minBitrate)},
        {DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE, std::to_string(maxBitrate)},
    };
}

AccountCodecInfo::AccountCodecInfo(const SystemCodecInfo& sysCodecInfo) noexcept
    : systemCodecInfo(sysCodecInfo)
    , payloadType(sysCodecInfo.payloadType)
    , bitrate(sysCodecInfo.bitrate)
{
    if (sysCodecInfo.minQuality != SystemCodecInfo::DEFAULT_NO_QUALITY)
        quality = SystemCodecInfo::DEFAULT_CODEC_QUALITY;
    else
        quality = SystemCodecInfo::DEFAULT_NO_QUALITY;
}

AccountCodecInfo&
AccountCodecInfo::operator=(const AccountCodecInfo& o)
{
    if (&systemCodecInfo != &o.systemCodecInfo)
        throw std::runtime_error("cannot assign codec info object pointing to another codec.");
    order = o.order;
    isActive = o.isActive;
    payloadType = o.payloadType;
    bitrate = o.bitrate;
    quality = o.quality;
    return *this;
}

AccountAudioCodecInfo::AccountAudioCodecInfo(const SystemAudioCodecInfo& sysCodecInfo)
    : AccountCodecInfo(sysCodecInfo)
    , audioformat {sysCodecInfo.audioformat}
{}

std::map<std::string, std::string>
AccountAudioCodecInfo::getCodecSpecifications()
{
    return {{DRing::Account::ConfProperties::CodecInfo::NAME, systemCodecInfo.name},
            {DRing::Account::ConfProperties::CodecInfo::TYPE,
             (systemCodecInfo.mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
            {DRing::Account::ConfProperties::CodecInfo::BITRATE, std::to_string(bitrate)},
            {DRing::Account::ConfProperties::CodecInfo::SAMPLE_RATE,
             std::to_string(audioformat.sample_rate)},
            {DRing::Account::ConfProperties::CodecInfo::CHANNEL_NUMBER,
             std::to_string(audioformat.nb_channels)}};
}

void
AccountAudioCodecInfo::setCodecSpecifications(const std::map<std::string, std::string>& details)
{
    decltype(bitrate) tmp_bitrate = jami::stoi(
        details.at(DRing::Account::ConfProperties::CodecInfo::BITRATE));
    decltype(audioformat) tmp_audioformat = audioformat;
    tmp_audioformat.sample_rate = jami::stoi(
        details.at(DRing::Account::ConfProperties::CodecInfo::SAMPLE_RATE));

    // copy back if no exception was raised
    bitrate = tmp_bitrate;
    audioformat = tmp_audioformat;
}

bool
AccountAudioCodecInfo::isPCMG722() const
{
    return systemCodecInfo.avcodecId == AV_CODEC_ID_ADPCM_G722;
}

AccountVideoCodecInfo::AccountVideoCodecInfo(const SystemVideoCodecInfo& sysCodecInfo)
    : AccountCodecInfo(sysCodecInfo)
    , frameRate(sysCodecInfo.frameRate)
    , profileId(sysCodecInfo.profileId)
{}

std::map<std::string, std::string>
AccountVideoCodecInfo::getCodecSpecifications()
{
    return {{DRing::Account::ConfProperties::CodecInfo::NAME, systemCodecInfo.name},
            {DRing::Account::ConfProperties::CodecInfo::TYPE,
             (systemCodecInfo.mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
            {DRing::Account::ConfProperties::CodecInfo::BITRATE, std::to_string(bitrate)},
            {DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE,
             std::to_string(systemCodecInfo.maxBitrate)},
            {DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE,
             std::to_string(systemCodecInfo.minBitrate)},
            {DRing::Account::ConfProperties::CodecInfo::QUALITY, std::to_string(quality)},
            {DRing::Account::ConfProperties::CodecInfo::MAX_QUALITY,
             std::to_string(systemCodecInfo.maxQuality)},
            {DRing::Account::ConfProperties::CodecInfo::MIN_QUALITY,
             std::to_string(systemCodecInfo.minQuality)},
            {DRing::Account::ConfProperties::CodecInfo::FRAME_RATE, std::to_string(frameRate)},
            {DRing::Account::ConfProperties::CodecInfo::AUTO_QUALITY_ENABLED,
             bool_to_str(isAutoQualityEnabled)}};
}

void
AccountVideoCodecInfo::setCodecSpecifications(const std::map<std::string, std::string>& details)
{
    auto copy = *this;

    auto it = details.find(DRing::Account::ConfProperties::CodecInfo::BITRATE);
    if (it != details.end())
        copy.bitrate = jami::stoi(it->second);

    it = details.find(DRing::Account::ConfProperties::CodecInfo::FRAME_RATE);
    if (it != details.end())
        copy.frameRate = jami::stoi(it->second);

    it = details.find(DRing::Account::ConfProperties::CodecInfo::QUALITY);
    if (it != details.end())
        copy.quality = jami::stoi(it->second);

    it = details.find(DRing::Account::ConfProperties::CodecInfo::AUTO_QUALITY_ENABLED);
    if (it != details.end())
        copy.isAutoQualityEnabled = (it->second == TRUE_STR) ? true : false;

    // copy back if no exception was raised
    *this = std::move(copy);
}

std::vector<MediaAttribute>
MediaAttribute::parseMediaList(const std::vector<MediaMap>& mediaList)
{
    std::vector<MediaAttribute> mediaAttrList;

    for (unsigned index = 0; index < mediaList.size(); index++) {
        MediaAttribute mediaAttr;
        std::pair<bool, MediaType> pairType = getMediaType(mediaList, index);
        if (pairType.first)
            mediaAttr.type_ = pairType.second;

        std::pair<bool, bool> pairBool;

        pairBool = getBoolValue(mediaList, index, MediaAttributeKey::SECURE);
        if (pairBool.first)
            mediaAttr.security_ = pairBool.second ? KeyExchangeProtocol::SDES
                                                  : KeyExchangeProtocol::NONE;

        pairBool = getBoolValue(mediaList, index, MediaAttributeKey::MUTED);
        if (pairBool.first)
            mediaAttr.muted_ = pairBool.second;

        pairBool = getBoolValue(mediaList, index, MediaAttributeKey::ENABLED);
        if (pairBool.first)
            mediaAttr.enabled_ = pairBool.second;

        std::pair<bool, std::string> pairString;
        pairString = getStringValue(mediaList, index, MediaAttributeKey::SOURCE);
        if (pairBool.first)
            mediaAttr.sourceUri_ = pairString.second;

        pairString = getStringValue(mediaList, index, MediaAttributeKey::LABEL);
        if (pairBool.first)
            mediaAttr.label_ = pairString.second;

        mediaAttrList.emplace_back(std::move(mediaAttr));
    }

    return mediaAttrList;
}

MediaType
MediaAttribute::stringToMediaType(const std::string& mediaType)
{
    if (mediaType.compare(MediaAttributeValue::AUDIO) == 0)
        return MediaType::MEDIA_AUDIO;
    if (mediaType.compare(MediaAttributeValue::VIDEO) == 0)
        return MediaType::MEDIA_VIDEO;
    return MediaType::MEDIA_NONE;
}

std::pair<bool, MediaType>
MediaAttribute::getMediaType(const std::vector<MediaMap>& mediaList, unsigned index)
{
    assert(index < mediaList.size());
    auto map = mediaList[index];

    const auto& iter = map.find(MediaAttributeKey::MEDIA_TYPE);
    if (iter == map.end()) {
        JAMI_WARN("[MEDIA_TYPE] key not found for media @ index %u", index);
        return {false, MediaType::MEDIA_NONE};
    }

    auto type = stringToMediaType(iter->second);
    if (type == MediaType::MEDIA_NONE) {
        JAMI_ERR("Invalid value [%s] for a media type key for media @ index %u",
                 iter->second.c_str(),
                 index);
        return {false, type};
    }

    return {true, type};
}

std::pair<bool, bool>
MediaAttribute::getBoolValue(const std::vector<MediaMap>& mediaList,
                             unsigned index,
                             const std::string& key)
{
    assert(index < mediaList.size());
    auto map = mediaList[index];

    const auto& iter = map.find(key);
    if (iter == map.end()) {
        JAMI_WARN("[%s] key not found for media @ index %u", key.c_str(), index);
        return {false, false};
    }
    auto const& value = iter->second;
    if (value.compare(MediaAttributeValue::TRUE_VAL) == 0)
        return {true, true};
    if (value.compare(MediaAttributeValue::FALSE_VAL) == 0)
        return {true, false};

    JAMI_ERR("Invalid value %s for a boolean key for media @ index %u", value.c_str(), index);
    return {false, false};
}

std::pair<bool, std::string>
MediaAttribute::getStringValue(const std::vector<MediaMap>& mediaList,
                               unsigned index,
                               const std::string& key)
{
    assert(index < mediaList.size());
    auto map = mediaList[index];

    const auto& iter = map.find(key);
    if (iter == map.end()) {
        JAMI_WARN("[%s] key not found for media @ index %u", key.c_str(), index);
        return {false, {}};
    }

    return {true, iter->second};
}

} // namespace jami
