/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author:  Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  Mostly borrowed from asterisk's sources (Steve Underwood <steveu@coppice.org>)
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

#include "audiocodec.h"
using std::ptrdiff_t;
#include <ccrtp/rtp.h>

namespace sfl {

AudioCodec::AudioCodec(uint8 payload, const std::string &codecName, int clockRate, int frameSize, unsigned channels) :
    codecName_(codecName),
    clockRate_(clockRate),
    channel_(channels),
    frameSize_(frameSize),
    bitrate_(0.0),
    payload_(payload),
    payloadFormat_(payload, clockRate_),
    hasDynamicPayload_((payload_ >= 96 and payload_ <= 127) or payload_ == 9)
{}

AudioCodec::AudioCodec(const AudioCodec& c) :
    codecName_(c.codecName_),
    clockRate_(c.clockRate_),
    channel_(c.channel_),
    frameSize_(c.frameSize_),
    bitrate_(c.bitrate_),
    payload_(c.payload_),
    payloadFormat_(c.payloadFormat_),
    hasDynamicPayload_(c.hasDynamicPayload_)
{}

int AudioCodec::decode(std::vector<std::vector<short> > *dst, unsigned char *buf, size_t buffer_size, size_t dst_offset /* = 0 */)
{
    //dst.setSampleRate(clockRate_);
    //return decode(&(*(dst.getChannel()->begin()+dst_offset)), buf, buffer_size);
    return decode(&(*((*dst)[0].begin()+dst_offset)), buf, buffer_size);
}

int AudioCodec::encode(unsigned char *dst, std::vector<std::vector<short> > *src, size_t buffer_size)
{
    //return encode(dst, src.getChannel()->data(), buffer_size);
    return encode(dst, (*src)[0].data(), buffer_size);
}

std::string AudioCodec::getMimeSubtype() const
{
    return codecName_;
}

uint8 AudioCodec::getPayloadType() const
{
    return payload_;
}

bool AudioCodec::hasDynamicPayload() const
{
    return hasDynamicPayload_;
}

uint32 AudioCodec::getClockRate() const
{
    return clockRate_;
}

unsigned AudioCodec::getFrameSize() const
{
    return frameSize_;
}

double AudioCodec::getBitRate() const
{
    return bitrate_;
}

unsigned AudioCodec::getChannels() const
{
    return channel_;
}

} // end namespace sfl
