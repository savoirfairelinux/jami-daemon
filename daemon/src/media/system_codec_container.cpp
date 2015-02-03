/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
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

#include "libav_deps.h"
#include "logger.h"
#include "system_codec_container.h"

namespace ring {

decltype(getGlobalInstance<SystemCodecContainer>)& getSystemCodecContainer = getGlobalInstance<SystemCodecContainer>;

SystemCodecContainer::SystemCodecContainer()
{
    initCodecConfig();
}
SystemCodecContainer::~SystemCodecContainer()
{
    //TODO
}
void
SystemCodecContainer::initCodecConfig()
{
    /* Define supported video codec*/
    std::shared_ptr<SystemVideoCodecInfo> codec_h264 (new SystemVideoCodecInfo(AV_CODEC_ID_H264, "H264","libx264", CODEC_ENCODER_DECODER));
    availableCodecList.push_back(codec_h264);

    std::shared_ptr<SystemVideoCodecInfo> codec_h263 (new SystemVideoCodecInfo(AV_CODEC_ID_H263P, "H263-2000","h263p", CODEC_ENCODER_DECODER));
    availableCodecList.push_back(codec_h263);

    //TODO : add last video codecs with correct params

    /* Define supported audio codec*/
    std::shared_ptr<SystemAudioCodecInfo> codec_pcma (new SystemAudioCodecInfo(AV_CODEC_ID_PCM_ALAW, "PCMA","pcma", CODEC_ENCODER_DECODER, 64, 8000, 1, 8));
    availableCodecList.push_back(codec_pcma);

    std::shared_ptr<SystemAudioCodecInfo> codec_opus (new SystemAudioCodecInfo(AV_CODEC_ID_OPUS, "opus","libopus", CODEC_ENCODER_DECODER, 0, 48000, 2, 104));
    availableCodecList.push_back(codec_opus);

    //TODO : add last audio codecs with correct params

    checkInstalledCodecs();
}

void
SystemCodecContainer::checkInstalledCodecs()
{
    AVCodec *codec = NULL;
    AVCodecID codecId;
    std::string codecName;
    CodecType codecType;

    for (auto codecIt : availableCodecList)
    {
        codecId = (AVCodecID)codecIt->avcodecId_;
        codecName = codecIt->name_;
        codecType = codecIt->codecType_;

        RING_INFO("Checking codec %s", codecName.c_str());

        if (codecType & CODEC_ENCODER)
        {
            codec = avcodec_find_encoder(codecId);
            if (!codec)
            {
                RING_ERR("Can not find encoder for codec  %s", codecName.c_str());
                codecIt->codecType_ = (CodecType)((unsigned)codecType & ~CODEC_ENCODER);
            } else {
                RING_INFO("### Found encoder %s", codecIt->to_string().c_str());
            }
        }

        if (codecType & CODEC_DECODER)
        {
            codec = avcodec_find_decoder(codecId);
            if (!codec)
            {
                RING_ERR("Can not find decoder for codec  %s", codecName.c_str());
                codecIt->codecType_ = (CodecType)((unsigned)codecType & ~CODEC_DECODER);
            } else {
                RING_INFO("### Found decoder %s", codecIt->to_string().c_str());
            }
        }
    }
}

std::vector<std::shared_ptr<SystemCodecInfo>>
SystemCodecContainer::getSystemCodecInfoList(MediaType mediaType)
{
    if (mediaType & MEDIA_ALL)
        return availableCodecList;

    //otherwise we have to instantiate a new list containing filtered objects
    // must be destroyed by the caller...
    std::vector<std::shared_ptr<SystemCodecInfo>> systemCodecList;
    for (auto codecIt : availableCodecList)
    {
        if (codecIt->mediaType_ & mediaType)
            systemCodecList.push_back(codecIt);
    }
    return systemCodecList;
}

std::vector<unsigned>
SystemCodecContainer::getSystemCodecInfoIdList(MediaType mediaType)
{
    std::vector<unsigned> idList;
    for (auto codecIt : availableCodecList)
    {
        if (codecIt->mediaType_ & mediaType)
            idList.push_back(codecIt->id_);
    }
    return idList;
}

std::shared_ptr<SystemCodecInfo>
SystemCodecContainer::searchCodecById(unsigned codecId, MediaType mediaType)
{
    for (auto codecIt : availableCodecList)
    {
        if ((codecIt->id_ == codecId) &&
                (codecIt->mediaType_ & mediaType ))
                return codecIt;
    }
    return nullptr;
}
std::shared_ptr<SystemCodecInfo>
SystemCodecContainer::searchCodecByName(std::string name, MediaType mediaType)
{
    for (auto codecIt : availableCodecList)
    {
        if ((codecIt->name_.compare(name) == 0) &&
                (codecIt->mediaType_ & mediaType ))
                return codecIt;
    }
    return nullptr;
}
std::shared_ptr<SystemCodecInfo>
SystemCodecContainer::searchCodecByPayload(unsigned payload, MediaType mediaType)
{
    for (auto codecIt : availableCodecList)
    {
        if ((codecIt->payloadType_ == payload ) &&
                (codecIt->mediaType_ & mediaType))
                return codecIt;
    }
    return nullptr;
}
}//namespace ring
