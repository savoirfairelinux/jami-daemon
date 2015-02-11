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
#include "media_codec.h"
#include <string.h>
#include <sstream>


namespace ring {
// TODO: initialize inside a function !
// put SystemCodecInfo in abstract
//static unsigned s_codecId = 1;

/*
 * SystemCodecInfo
 */
SystemCodecInfo::SystemCodecInfo(AVCodecID avcodecId, const std::string name, std::string libName,
        MediaType mediaType, CodecType codecType, unsigned bitrate, unsigned payloadType)
    : id_(generateId()), avcodecId_(avcodecId), name_(name), payloadType_(payloadType)
    , libName_(libName), bitrate_(bitrate), codecType_(codecType), mediaType_(mediaType)
{}

SystemCodecInfo::~SystemCodecInfo()
{}

std::string SystemCodecInfo::to_string() const
{
    std::ostringstream out;
    out << " type:" << (unsigned)codecType_ << " , avcodecID:" << avcodecId_
        << " ,name:" << name_ << " ,PT:" << payloadType_ << " ,libName:" << libName_ << " ,bitrate:" << bitrate_;

    return out.str();
}

/*
 * SystemAudioCodecInfo
 */

SystemAudioCodecInfo::SystemAudioCodecInfo(AVCodecID avcodecId, const std::string name, std::string libName,
        CodecType type, unsigned bitrate, unsigned sampleRate, unsigned nbChannels, unsigned payloadType)
    : SystemCodecInfo(avcodecId, name, libName, MEDIA_AUDIO, type, bitrate, payloadType)
    , sampleRate_(sampleRate), nbChannels_(nbChannels)
{}

SystemAudioCodecInfo::~SystemAudioCodecInfo()
{}

std::vector<std::string> SystemAudioCodecInfo::getCodecSpecifications()
{
    //FORMAT: list of
    //  * name of the codec
    //  * sample rate
    //  * bit rate
    //  * channel number

     std::vector< std::string > listSpec;
     listSpec.push_back(name_);
     listSpec.push_back(std::to_string(sampleRate_));
     listSpec.push_back(std::to_string(bitrate_));
     listSpec.push_back(std::to_string(nbChannels_));
     return listSpec;
}

/*
 * SystemVideoCodecInfo
 */
SystemVideoCodecInfo::SystemVideoCodecInfo(AVCodecID avcodecId, const std::string name, std::string libName,
        CodecType type, unsigned payloadType)
    : SystemCodecInfo(avcodecId, name, libName, MEDIA_VIDEO, type, payloadType)
{}

SystemVideoCodecInfo::~SystemVideoCodecInfo()
{}

std::vector<std::string> SystemVideoCodecInfo::getCodecSpecifications()
{
    //FORMAT: list of
    //  * name of the codec
    //  * bit rate
    //  * parameters

     std::vector< std::string > listSpec;
     listSpec.push_back(name_);
     listSpec.push_back(std::to_string(bitrate_));
     listSpec.push_back(parameters_);
     return listSpec;
}

AccountCodecInfo::AccountCodecInfo(const SystemCodecInfo& sysCodecInfo)
    : systemCodecInfo(sysCodecInfo)
    ,order_(0)
    ,isActive_(true)
    ,payloadType_(sysCodecInfo.payloadType_)
    ,bitrate_(sysCodecInfo.bitrate_)
{}

AccountCodecInfo::~AccountCodecInfo()
{}

AccountAudioCodecInfo::AccountAudioCodecInfo(const SystemAudioCodecInfo& sysCodecInfo)
    : AccountCodecInfo(sysCodecInfo)
    ,sampleRate_(sysCodecInfo.sampleRate_)
    ,nbChannels_(sysCodecInfo.nbChannels_)
{}

AccountAudioCodecInfo::~AccountAudioCodecInfo()
{}

AccountVideoCodecInfo::AccountVideoCodecInfo(const SystemVideoCodecInfo& sysCodecInfo)
    : AccountCodecInfo(sysCodecInfo)
    ,frameRate_(sysCodecInfo.frameRate_)
    ,profileId_(sysCodecInfo.profileId_)
{}

AccountVideoCodecInfo::~AccountVideoCodecInfo()
{}

static unsigned& generateId()
{
    static unsigned id = 0 ;
    return id;
}
}//namespace ring
