/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "logger.h"
#include "system_codec_container.h"
#include "media_encoder.h"

#include <sstream>

#ifdef APPLE
#include <TargetConditionals.h>
#endif

namespace jami {

decltype(getGlobalInstance<SystemCodecContainer>)& getSystemCodecContainer
    = getGlobalInstance<SystemCodecContainer>;

SystemCodecContainer::SystemCodecContainer()
{
    initCodecConfig();
}

SystemCodecContainer::~SystemCodecContainer()
{
    // TODO
}

void
SystemCodecContainer::initCodecConfig()
{
#ifdef ENABLE_VIDEO
    auto minH264 = SystemCodecInfo::DEFAULT_H264_MIN_QUALITY;
    auto maxH264 = SystemCodecInfo::DEFAULT_H264_MAX_QUALITY;
    auto minH265 = SystemCodecInfo::DEFAULT_H264_MIN_QUALITY;
    auto maxH265 = SystemCodecInfo::DEFAULT_H264_MAX_QUALITY;
    auto minVP8 = SystemCodecInfo::DEFAULT_VP8_MIN_QUALITY;
    auto maxVP8 = SystemCodecInfo::DEFAULT_VP8_MAX_QUALITY;
    auto defaultBitrate = SystemCodecInfo::DEFAULT_VIDEO_BITRATE;
#endif
    availableCodecList_ = {
#ifdef ENABLE_VIDEO
        /* Define supported video codec*/
        std::make_shared<SystemVideoCodecInfo>(AV_CODEC_ID_HEVC,
                                               AV_CODEC_ID_HEVC,
                                               "H.265/HEVC",
                                               "H265",
                                               "",
                                               CODEC_ENCODER_DECODER,
                                               defaultBitrate,
                                               minH265,
                                               maxH265),

        std::make_shared<SystemVideoCodecInfo>(AV_CODEC_ID_H264,
                                               AV_CODEC_ID_H264,
                                               "H.264/AVC",
                                               "H264",
                                               "libx264",
                                               CODEC_ENCODER_DECODER,
                                               defaultBitrate,
                                               minH264,
                                               maxH264),

        std::make_shared<SystemVideoCodecInfo>(AV_CODEC_ID_VP8,
                                               AV_CODEC_ID_VP8,
                                               "VP8",
                                               "VP8",
                                               "libvpx",
                                               CODEC_ENCODER_DECODER,
                                               defaultBitrate,
                                               minVP8,
                                               maxVP8),
#if !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)
        std::make_shared<SystemVideoCodecInfo>(AV_CODEC_ID_MPEG4,
                                               AV_CODEC_ID_MPEG4,
                                               "MP4V-ES",
                                               "MP4V-ES",
                                               "mpeg4",
                                               CODEC_ENCODER_DECODER,
                                               defaultBitrate),

        std::make_shared<SystemVideoCodecInfo>(AV_CODEC_ID_H263,
                                               AV_CODEC_ID_H263,
                                               "H.263",
                                               "H263-1998",
                                               "h263",
                                               CODEC_ENCODER_DECODER,
                                               defaultBitrate),
#endif

#endif
        /* Define supported audio codec*/

        std::make_shared<SystemAudioCodecInfo>(AV_CODEC_ID_OPUS,
                                               AV_CODEC_ID_OPUS,
                                               "Opus",
                                               "opus",
                                               "libopus",
                                               CODEC_ENCODER_DECODER,
                                               0,
                                               48000,
                                               2,
                                               104),

        std::make_shared<SystemAudioCodecInfo>(AV_CODEC_ID_ADPCM_G722,
                                               AV_CODEC_ID_ADPCM_G722,
                                               "G.722",
                                               "G722",
                                               "g722",
                                               CODEC_ENCODER_DECODER,
                                               0,
                                               16000,
                                               1,
                                               9),

        std::make_shared<SystemAudioCodecInfo>(AV_CODEC_ID_ADPCM_G726,
                                               AV_CODEC_ID_ADPCM_G726,
                                               "G.726",
                                               "G726-32",
                                               "g726",
                                               CODEC_ENCODER_DECODER,
                                               0,
                                               8000,
                                               1,
                                               2),

        std::make_shared<SystemAudioCodecInfo>(AV_CODEC_ID_SPEEX | 0x20000000,
                                               AV_CODEC_ID_SPEEX,
                                               "Speex",
                                               "speex",
                                               "libspeex",
                                               CODEC_ENCODER_DECODER,
                                               0,
                                               32000,
                                               1,
                                               112),

        std::make_shared<SystemAudioCodecInfo>(AV_CODEC_ID_SPEEX | 0x10000000,
                                               AV_CODEC_ID_SPEEX,
                                               "Speex",
                                               "speex",
                                               "libspeex",
                                               CODEC_ENCODER_DECODER,
                                               0,
                                               16000,
                                               1,
                                               111),

        std::make_shared<SystemAudioCodecInfo>(AV_CODEC_ID_SPEEX,
                                               AV_CODEC_ID_SPEEX,
                                               "Speex",
                                               "speex",
                                               "libspeex",
                                               CODEC_ENCODER_DECODER,
                                               0,
                                               8000,
                                               1,
                                               110),

        std::make_shared<SystemAudioCodecInfo>(AV_CODEC_ID_PCM_ALAW,
                                               AV_CODEC_ID_PCM_ALAW,
                                               "G.711a",
                                               "PCMA",
                                               "pcm_alaw",
                                               CODEC_ENCODER_DECODER,
                                               64,
                                               8000,
                                               1,
                                               8),

        std::make_shared<SystemAudioCodecInfo>(AV_CODEC_ID_PCM_MULAW,
                                               AV_CODEC_ID_PCM_MULAW,
                                               "G.711u",
                                               "PCMU",
                                               "pcm_mulaw",
                                               CODEC_ENCODER_DECODER,
                                               64,
                                               8000,
                                               1,
                                               0),
    };
    setActiveH265();
    checkInstalledCodecs();
}

bool
SystemCodecContainer::setActiveH265()
{
#if (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    removeCodecByName("H265");
    return false;
#endif

    auto apiName = MediaEncoder::testH265Accel();
    if (apiName != "") {
        JAMI_WARN("Found a usable accelerated H265/HEVC codec: %s, enabling.", apiName.c_str());
        return true;
    } else {
        JAMI_ERR("Can't find a usable accelerated H265/HEVC codec, disabling.");
        removeCodecByName("H265");
    }
    return false;
}

void
SystemCodecContainer::checkInstalledCodecs()
{
    std::ostringstream enc_ss;
    std::ostringstream dec_ss;

    for (const auto& codecIt : availableCodecList_) {
        AVCodecID codecId = (AVCodecID) codecIt->avcodecId;
        CodecType codecType = codecIt->codecType;

        if (codecType & CODEC_ENCODER) {
            if (avcodec_find_encoder(codecId) != nullptr)
                enc_ss << codecIt->name << ' ';
            else
                codecIt->codecType = (CodecType)((unsigned) codecType & ~CODEC_ENCODER);
        }

        if (codecType & CODEC_DECODER) {
            if (avcodec_find_decoder(codecId) != nullptr)
                dec_ss << codecIt->name << ' ';
            else
                codecIt->codecType = (CodecType)((unsigned) codecType & ~CODEC_DECODER);
        }
    }
    JAMI_INFO("Encoders found: %s", enc_ss.str().c_str());
    JAMI_INFO("Decoders found: %s", dec_ss.str().c_str());
}

std::vector<std::shared_ptr<SystemCodecInfo>>
SystemCodecContainer::getSystemCodecInfoList(MediaType mediaType)
{
    if (mediaType & MEDIA_ALL)
        return availableCodecList_;

    // otherwise we have to instantiate a new list containing filtered objects
    // must be destroyed by the caller...
    std::vector<std::shared_ptr<SystemCodecInfo>> systemCodecList;
    for (const auto& codecIt : availableCodecList_) {
        if (codecIt->mediaType & mediaType)
            systemCodecList.push_back(codecIt);
    }
    return systemCodecList;
}

std::vector<unsigned>
SystemCodecContainer::getSystemCodecInfoIdList(MediaType mediaType)
{
    std::vector<unsigned> idList;
    for (const auto& codecIt : availableCodecList_) {
        if (codecIt->mediaType & mediaType)
            idList.push_back(codecIt->id);
    }
    return idList;
}

std::shared_ptr<SystemCodecInfo>
SystemCodecContainer::searchCodecById(unsigned codecId, MediaType mediaType)
{
    for (const auto& codecIt : availableCodecList_) {
        if ((codecIt->id == codecId) && (codecIt->mediaType & mediaType))
            return codecIt;
    }
    return {};
}
std::shared_ptr<SystemCodecInfo>
SystemCodecContainer::searchCodecByName(const std::string& name, MediaType mediaType)
{
    for (const auto& codecIt : availableCodecList_) {
        if (codecIt->name == name && (codecIt->mediaType & mediaType))
            return codecIt;
    }
    return {};
}
std::shared_ptr<SystemCodecInfo>
SystemCodecContainer::searchCodecByPayload(unsigned payload, MediaType mediaType)
{
    for (const auto& codecIt : availableCodecList_) {
        if ((codecIt->payloadType == payload) && (codecIt->mediaType & mediaType))
            return codecIt;
    }
    return {};
}
void
SystemCodecContainer::removeCodecByName(const std::string& name, MediaType mediaType)
{
    for (auto codecIt = availableCodecList_.begin(); codecIt != availableCodecList_.end();
         ++codecIt) {
        if ((*codecIt)->mediaType & mediaType and (*codecIt)->name == name) {
            availableCodecList_.erase(codecIt);
            break;
        }
    }
}

} // namespace jami
