/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include <cassert>
using std::ptrdiff_t;

namespace sfl {

AudioCodec::AudioCodec(uint8_t payload, const std::string &codecName, uint32_t clockRate, unsigned frameSize, uint8_t channels) :
    codecName_(codecName),
    clockRate_(clockRate),
    clockRateCur_(clockRate),
    channels_(channels),
    channelsCur_(channels),
    frameSize_(frameSize),
    bitrate_(0.0),
    payload_(payload),
    hasDynamicPayload_((payload_ >= 96 and payload_ <= 127) or payload_ == 9)
{}

AudioCodec::AudioCodec(const AudioCodec& c) :
    codecName_(c.codecName_),
    clockRate_(c.clockRate_),
    clockRateCur_(c.clockRateCur_),
    channels_(c.channels_),
    channelsCur_(c.channelsCur_),
    frameSize_(c.frameSize_),
    bitrate_(c.bitrate_),
    payload_(c.payload_),
    hasDynamicPayload_(c.hasDynamicPayload_)
{}

int AudioCodec::decode(SFLAudioSample *, unsigned char *, size_t)
{
    // Unimplemented!
    assert(false);
    return 0;
}

int AudioCodec::encode(unsigned char *, SFLAudioSample *, size_t)
{
    // Unimplemented!
    assert(false);
    return 0;
}


// Mono only, subclasses must implement multichannel support
int AudioCodec::decode(std::vector<std::vector<SFLAudioSample> > &pcm, const uint8_t* data, size_t len)
{
    return decode(pcm[0].data(), const_cast<uint8_t*>(data), len);
}

// Mono only, subclasses must implement multichannel support
size_t AudioCodec::encode(const std::vector<std::vector<SFLAudioSample> > &pcm, uint8_t *data, size_t len)
{
    return encode(data, const_cast<SFLAudioSample*>(pcm[0].data()), len);
}

int AudioCodec::decode(std::vector<std::vector<SFLAudioSample> > &pcm)
{
    pcm.clear();
    return frameSize_;
}

std::string AudioCodec::getMimeSubtype() const
{
    return codecName_;
}

uint8_t AudioCodec::getPayloadType() const
{
    return payload_;
}

bool AudioCodec::hasDynamicPayload() const
{
    return hasDynamicPayload_;
}

uint32_t AudioCodec::getClockRate() const
{
    return clockRate_;
}

uint32_t AudioCodec::getCurrentClockRate() const
{
    return clockRateCur_;
}

uint32_t AudioCodec::getSDPClockRate() const
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

uint8_t AudioCodec::getChannels() const
{
    return channels_;
}

uint8_t AudioCodec::getCurrentChannels() const
{
    return channelsCur_;
}

const char *
AudioCodec::getSDPChannels() const
{
    return "";
}

} // end namespace sfl
