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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "media_codec.h"
#include "account_const.h"

#include "string_utils.h"

#include <string>
#include <sstream>

#define DEFAULT_MIN_BITRATE 200
#define DEFAULT_MAX_BITRATE 4000
namespace ring {

static unsigned&
generateId()
{
    static unsigned id = 0;
    return ++id;
}

/*
 * SystemCodecInfo
 */
SystemCodecInfo::SystemCodecInfo(unsigned avcodecId, const std::string name,
                                 std::string libName,
                                 MediaType mediaType, CodecType codecType,
                                 unsigned bitrate, unsigned payloadType)
    : id(generateId())
    , avcodecId(avcodecId)
    , name(name)
    , libName(libName)
    , codecType(codecType)
    , mediaType(mediaType)
    , payloadType(payloadType)
    , bitrate(bitrate)
    , minBitrate(DEFAULT_MIN_BITRATE)
    , maxBitrate(DEFAULT_MAX_BITRATE)
{}

SystemCodecInfo::~SystemCodecInfo()
{}

std::string
SystemCodecInfo::to_string() const
{
    std::ostringstream out;
    out << " type:" << (unsigned)codecType
        << " , avcodecID:" << avcodecId
        << " ,name:" << name
        << " ,PT:" << payloadType
        << " ,libName:" << libName
        << " ,bitrate:" << bitrate;

    return out.str();
}

/*
 * SystemAudioCodecInfo
 */
SystemAudioCodecInfo::SystemAudioCodecInfo(unsigned m_avcodecId,
                                           const std::string m_name,
                                           std::string m_libName,
                                           CodecType m_type, unsigned m_bitrate,
                                           unsigned m_sampleRate,
                                           unsigned m_nbChannels,
                                           unsigned m_payloadType)
    : SystemCodecInfo(m_avcodecId, m_name, m_libName, MEDIA_AUDIO, m_type, m_bitrate, m_payloadType)
    , audioformat{m_sampleRate, m_nbChannels}
{}

SystemAudioCodecInfo::~SystemAudioCodecInfo()
{}


std::map<std::string, std::string>
SystemAudioCodecInfo::getCodecSpecifications()
{
    return {
        {DRing::Account::ConfProperties::CodecInfo::NAME, name},
        {DRing::Account::ConfProperties::CodecInfo::TYPE, (mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
        {DRing::Account::ConfProperties::CodecInfo::BITRATE, ring::to_string(bitrate)},
        {DRing::Account::ConfProperties::CodecInfo::SAMPLE_RATE, ring::to_string(audioformat.sample_rate)},
        {DRing::Account::ConfProperties::CodecInfo::CHANNEL_NUMBER, ring::to_string(audioformat.nb_channels)}
        };
}

/*
 * SystemVideoCodecInfo
 */
SystemVideoCodecInfo::SystemVideoCodecInfo(unsigned m_avcodecId,
                                           const std::string m_name,
                                           std::string m_libName,
                                           CodecType m_type,
                                           unsigned m_bitrate,
                                           unsigned m_payloadType,
                                           unsigned m_frameRate,
                                           unsigned m_profileId)
    : SystemCodecInfo(m_avcodecId, m_name, m_libName, MEDIA_VIDEO,
                      m_type, m_bitrate, m_payloadType)
    , frameRate(m_frameRate), profileId(m_profileId)
{}

SystemVideoCodecInfo::~SystemVideoCodecInfo()
{}

std::map<std::string, std::string>
SystemVideoCodecInfo::getCodecSpecifications()
{
    return {
        {DRing::Account::ConfProperties::CodecInfo::NAME, name},
        {DRing::Account::ConfProperties::CodecInfo::TYPE, (mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
        {DRing::Account::ConfProperties::CodecInfo::BITRATE, ring::to_string(bitrate)},
        {DRing::Account::ConfProperties::CodecInfo::FRAME_RATE, ring::to_string(frameRate)},
        {DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE, ring::to_string(minBitrate)},
        {DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE, ring::to_string(maxBitrate)},
    };
}

AccountCodecInfo::AccountCodecInfo(const SystemCodecInfo& sysCodecInfo)
    : systemCodecInfo(sysCodecInfo)
    , order(0)
    , isActive(true)
    , payloadType(sysCodecInfo.payloadType)
    , bitrate(sysCodecInfo.bitrate)
{}

AccountCodecInfo::~AccountCodecInfo()
{}

AccountAudioCodecInfo::AccountAudioCodecInfo(const SystemAudioCodecInfo& sysCodecInfo)
    : AccountCodecInfo(sysCodecInfo)
    , audioformat{sysCodecInfo.audioformat}
{}

std::map<std::string, std::string>
AccountAudioCodecInfo::getCodecSpecifications()
{
    return {
        {DRing::Account::ConfProperties::CodecInfo::NAME, systemCodecInfo.name},
        {DRing::Account::ConfProperties::CodecInfo::TYPE, (systemCodecInfo.mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
        {DRing::Account::ConfProperties::CodecInfo::BITRATE, ring::to_string(bitrate)},
        {DRing::Account::ConfProperties::CodecInfo::SAMPLE_RATE, ring::to_string(audioformat.sample_rate)},
        {DRing::Account::ConfProperties::CodecInfo::CHANNEL_NUMBER, ring::to_string(audioformat.nb_channels)}
        };
}

void
AccountAudioCodecInfo::setCodecSpecifications(const std::map<std::string, std::string>& details)
{
    auto it = details.find(DRing::Account::ConfProperties::CodecInfo::BITRATE);
    if (it != details.end())
        bitrate = ring::stoi(it->second);

    it = details.find(DRing::Account::ConfProperties::CodecInfo::SAMPLE_RATE);
    if (it != details.end())
        audioformat.sample_rate = ring::stoi(it->second);

    it = details.find(DRing::Account::ConfProperties::CodecInfo::CHANNEL_NUMBER);
    if (it != details.end())
        audioformat.nb_channels = ring::stoi(it->second);
}

bool
AccountAudioCodecInfo::isPCMG722() const
{
    return systemCodecInfo.avcodecId == AV_CODEC_ID_ADPCM_G722;
}


AccountAudioCodecInfo::~AccountAudioCodecInfo()
{}

AccountVideoCodecInfo::AccountVideoCodecInfo(const SystemVideoCodecInfo& sysCodecInfo)
    : AccountCodecInfo(sysCodecInfo)
    , frameRate(sysCodecInfo.frameRate)
    , profileId(sysCodecInfo.profileId)
{}

std::map<std::string, std::string>
AccountVideoCodecInfo::getCodecSpecifications()
{
    return {
        {DRing::Account::ConfProperties::CodecInfo::NAME, systemCodecInfo.name},
        {DRing::Account::ConfProperties::CodecInfo::TYPE, (systemCodecInfo.mediaType & MEDIA_AUDIO ? "AUDIO" : "VIDEO")},
        {DRing::Account::ConfProperties::CodecInfo::BITRATE, ring::to_string(bitrate)},
        {DRing::Account::ConfProperties::CodecInfo::MAX_BITRATE, ring::to_string(systemCodecInfo.maxBitrate)},
        {DRing::Account::ConfProperties::CodecInfo::MIN_BITRATE, ring::to_string(systemCodecInfo.minBitrate)},
        {DRing::Account::ConfProperties::CodecInfo::FRAME_RATE, ring::to_string(frameRate)}
        };
}

void
AccountVideoCodecInfo::setCodecSpecifications(const std::map<std::string, std::string>& details)
{
    auto it = details.find(DRing::Account::ConfProperties::CodecInfo::BITRATE);
    if (it != details.end())
        bitrate = ring::stoi(it->second);

    it = details.find(DRing::Account::ConfProperties::CodecInfo::FRAME_RATE);
    if (it != details.end())
        frameRate = ring::stoi(it->second);
}

AccountVideoCodecInfo::~AccountVideoCodecInfo()
{}

} // namespace ring
