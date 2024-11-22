/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
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
                                 const std::string& longName,
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
    , longName(longName)
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

std::map<std::string, std::string>
SystemCodecInfo::getCodecSpecifications() const
{
    return {{libjami::Account::ConfProperties::CodecInfo::NAME, longName},
            {libjami::Account::ConfProperties::CodecInfo::TYPE,
             (mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
            {libjami::Account::ConfProperties::CodecInfo::BITRATE, std::to_string(bitrate)}};
}

/*
 * SystemAudioCodecInfo
 */
SystemAudioCodecInfo::SystemAudioCodecInfo(unsigned codecId,
                                           unsigned m_avcodecId,
                                           const std::string& longName,
                                           const std::string& m_name,
                                           const std::string& m_libName,
                                           CodecType m_type,
                                           unsigned m_bitrate,
                                           unsigned m_sampleRate,
                                           unsigned m_nbChannels,
                                           unsigned m_payloadType,
                                           AVSampleFormat sampleFormat)
    : SystemCodecInfo(codecId,
                      m_avcodecId,
                      longName,
                      m_name,
                      m_libName,
                      MEDIA_AUDIO,
                      m_type,
                      m_bitrate,
                      m_payloadType)
    , audioformat {m_sampleRate, m_nbChannels, sampleFormat}
{}

SystemAudioCodecInfo::~SystemAudioCodecInfo() {}

std::map<std::string, std::string>
SystemAudioCodecInfo::getCodecSpecifications() const
{
    return {{libjami::Account::ConfProperties::CodecInfo::NAME, longName},
            {libjami::Account::ConfProperties::CodecInfo::TYPE,
             (mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
            {libjami::Account::ConfProperties::CodecInfo::BITRATE, std::to_string(bitrate)},
            {libjami::Account::ConfProperties::CodecInfo::SAMPLE_RATE,
             std::to_string(audioformat.sample_rate)},
            {libjami::Account::ConfProperties::CodecInfo::CHANNEL_NUMBER,
             std::to_string(audioformat.nb_channels)}};
}


bool
SystemAudioCodecInfo::isPCMG722() const
{
    return avcodecId == AV_CODEC_ID_ADPCM_G722;
}

void
SystemAudioCodecInfo::setCodecSpecifications(const std::map<std::string, std::string>& details)
{
    decltype(bitrate) tmp_bitrate = std::stoi(
        details.at(libjami::Account::ConfProperties::CodecInfo::BITRATE));
    decltype(audioformat) tmp_audioformat = audioformat;
    tmp_audioformat.sample_rate = std::stoi(
        details.at(libjami::Account::ConfProperties::CodecInfo::SAMPLE_RATE));

    // copy back if no exception was raised
    bitrate = tmp_bitrate;
    audioformat = tmp_audioformat;
}

/*
 * SystemVideoCodecInfo
 */
SystemVideoCodecInfo::SystemVideoCodecInfo(unsigned codecId,
                                           unsigned m_avcodecId,
                                           const std::string& longName,
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
                      longName,
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
SystemVideoCodecInfo::getCodecSpecifications() const
{
    return {
        {libjami::Account::ConfProperties::CodecInfo::NAME, longName},
        {libjami::Account::ConfProperties::CodecInfo::TYPE,
         (mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
        {libjami::Account::ConfProperties::CodecInfo::BITRATE, std::to_string(bitrate)},
        {libjami::Account::ConfProperties::CodecInfo::FRAME_RATE, std::to_string(frameRate)},
        {libjami::Account::ConfProperties::CodecInfo::MIN_BITRATE, std::to_string(minBitrate)},
        {libjami::Account::ConfProperties::CodecInfo::MAX_BITRATE, std::to_string(maxBitrate)},
    };
}

void
SystemVideoCodecInfo::setCodecSpecifications(const std::map<std::string, std::string>& details)
{
    auto copy = *this;

    auto it = details.find(libjami::Account::ConfProperties::CodecInfo::BITRATE);
    if (it != details.end())
        copy.bitrate = std::stoi(it->second);

    it = details.find(libjami::Account::ConfProperties::CodecInfo::FRAME_RATE);
    if (it != details.end())
        copy.frameRate = std::stoi(it->second);

    it = details.find(libjami::Account::ConfProperties::CodecInfo::QUALITY);
    if (it != details.end())
        copy.quality = std::stoi(it->second);

    it = details.find(libjami::Account::ConfProperties::CodecInfo::AUTO_QUALITY_ENABLED);
    if (it != details.end())
        copy.isAutoQualityEnabled = (it->second == TRUE_STR) ? true : false;

    // copy back if no exception was raised
    *this = std::move(copy);
}
} // namespace jami
