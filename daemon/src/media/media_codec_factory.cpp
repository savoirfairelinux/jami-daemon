/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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
#include "media_codec_factory.h"
#include <iostream>
#include <unistd.h>

namespace ring {

decltype(getGlobalInstance<MediaCodecFactory>)& getMediaCodecFactory = getGlobalInstance<MediaCodecFactory>;

MediaCodecFactory::MediaCodecFactory()
{
    initCodecConfig();
}
MediaCodecFactory::~MediaCodecFactory()
{
    //TODO
}
void
MediaCodecFactory::initCodecConfig()
{
    availableCodecList = new std::vector<ring::MediaCodec*>;
    /* Define supported video codec*/
    MediaVideoCodec*  mediaVideoCodec = NULL;
    mediaVideoCodec = new MediaVideoCodec(AV_CODEC_ID_H264, "H264","libx264", CODEC_TYPE_ENCODER_DECODER);
    addCodec(mediaVideoCodec);
    mediaVideoCodec = new MediaVideoCodec(AV_CODEC_ID_H263P, "H263-2000","h263p", CODEC_TYPE_ENCODER_DECODER);
    addCodec(mediaVideoCodec);
    /*mediaVideoCodec = new MediaVideoCodec("VP8","libvpx");
    supportedVideoCodecList.push_back(mediaVideoCodec);
    mediaVideoCodec = new MediaVideoCodec("MP4V-ES","mpeg4");
    supportedVideoCodecList.push_back(mediaVideoCodec);*/

    /* Define supported audio codec*/
    MediaAudioCodec*  mediaAudioCodec = NULL;
    mediaAudioCodec = new MediaAudioCodec(AV_CODEC_ID_PCM_ALAW, "PCMA","PCM A-law", CODEC_TYPE_ENCODER_DECODER, 64,48000,1,8);
    addCodec(mediaAudioCodec);

    mediaAudioCodec = new MediaAudioCodec(AV_CODEC_ID_OPUS, "opus","libopus", CODEC_TYPE_ENCODER_DECODER, 0, 48000,2,104);
    addCodec(mediaAudioCodec);


    checkInstalledCodecs();
}

void
MediaCodecFactory::checkInstalledCodecs()
{
    //TODO: dont use avcodecId_ as integer
    //use AvCodecId enum
    AVCodec *codec = NULL;
    bool isEncoder = false;
    bool isDecoder = false;

    AVCodecID *codecId;
    std::string *codecName;
    CODEC_TYPE *codecType;
    uint16_t codecTypeInt;

    for (int i = 0; i < availableCodecList->size(); i++)
    {
        codecId = &availableCodecList->at(i)->avcodecId_;
        codecName = &availableCodecList->at(i)->name_;
        codecType = &availableCodecList->at(i)->codecType_;

        RING_INFO("Checking codec %s", codecName->c_str());

        if (*codecType & CODEC_TYPE_ENCODER)
        {
            codec = avcodec_find_encoder(*codecId);
            if (!codec)
            {
                RING_ERR("Can not find encoder for codec  %s", codecName->c_str());
                codecTypeInt = ((uint16_t) *codecType) & ~CODEC_TYPE_ENCODER;
                availableCodecList->at(i)->codecType_ = (CODEC_TYPE) codecTypeInt;
            } else {
                RING_INFO("### Found encoder %s", availableCodecList->at(i)->to_string().c_str());
            }
        }

        if (*codecType & CODEC_TYPE_DECODER)
        {
            codec = avcodec_find_decoder(*codecId);
            if (!codec)
            {
                RING_ERR("Can not find decoder for codec  %s", codecName->c_str());
                codecTypeInt = ((uint16_t) *codecType) & ~CODEC_TYPE_DECODER;
                availableCodecList->at(i)->codecType_ = (CODEC_TYPE) codecTypeInt;
            } else {
                RING_INFO("### Found decoder %s", availableCodecList->at(i)->to_string().c_str());
            }
        }
    }
}

void
MediaCodecFactory::addCodec(MediaCodec* c)
{
    availableCodecList->push_back(c);
}

std::vector<MediaCodec*> *
MediaCodecFactory::getMediaCodecList(MEDIA_TYPE mediaType)
{
    std::vector<MediaCodec*> * mediaCodecList;
    for ( uint16_t i = 0; i < availableCodecList->size(); i++)
    {
        if (availableCodecList->at(i)->mediaType_ & mediaType)
            mediaCodecList->push_back( availableCodecList->at(i));
    }
    return mediaCodecList;
}
std::vector<int32_t>
MediaCodecFactory::getMediaCodecIdList(MEDIA_TYPE mediaType)
{
    std::vector<int32_t> idList;
    for ( uint16_t i = 0; i < availableCodecList->size(); i++)
    {
        if (availableCodecList->at(i)->mediaType_ & mediaType)
            idList.push_back( availableCodecList->at(i)->getCodecId());
    }
    return idList;
}
ring::MediaCodec*
MediaCodecFactory::searchCodecById(uint16_t codecId, MEDIA_TYPE mediaType)
{
    ring::MediaCodec* foundCodec = NULL;
    for ( uint16_t i = 0; i < availableCodecList->size(); i++)
    {
        if ((availableCodecList->at(i)->codecId_ == codecId) &&
                ( availableCodecList->at(i)->mediaType_ ))
            foundCodec = availableCodecList->at(i);
    }
    return foundCodec;
}
ring::MediaCodec*
MediaCodecFactory::searchCodecByName(std::string name, MEDIA_TYPE mediaType)
{
    ring::MediaCodec* foundCodec = NULL;
    for ( uint16_t i = 0; i < availableCodecList->size(); i++)
    {
        if (((availableCodecList->at(i)->name_).compare(name) == 0) &&
                ( availableCodecList->at(i)->mediaType_ & mediaType ))
            foundCodec = availableCodecList->at(i);
    }
    return foundCodec;
}
ring::MediaCodec*
MediaCodecFactory::searchCodecByPayload(uint16_t payload, MEDIA_TYPE mediaType)
{
    ring::MediaCodec* foundCodec = NULL;
    for ( uint16_t i = 0; i < availableCodecList->size(); i++)
    {
        if ((availableCodecList->at(i)->payloadType_ == payload ) &&
                ( availableCodecList->at(i)->mediaType_ & mediaType))
            foundCodec = availableCodecList->at(i);
    }
    return foundCodec;
}

}
