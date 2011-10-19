/*
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 * Author:  Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 * Mostly borrowed from asterisk's sources (Steve Underwood <steveu@coppice.org>)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

AudioCodec::AudioCodec(uint8 payload, const std::string &codecName) :
    codecName_(codecName), clockRate_(8000), channel_(1), bitrate_(0.0),
    hasDynamicPayload_(false), payload_(payload)
{
    init(payload, clockRate_);
}

AudioCodec::AudioCodec(const AudioCodec& codec) :
    codecName_(codec.codecName_), clockRate_(codec.clockRate_),
    channel_(codec.channel_), bitrate_(codec.bitrate_),
    hasDynamicPayload_(false), payload_(codec.payload_)
{
    init(codec.payload_, codec.clockRate_);
}

void AudioCodec::init(uint8 payloadType, uint32 clockRate)
{
    payloadFormat_ = new ost::DynamicPayloadFormat(payloadType, clockRate);

    // If g722 (payload 9), we need to init libccrtp symetric sessions with using
    // dynamic payload format. This way we get control on rtp clockrate.
    hasDynamicPayload_ = ((payload_ >= 96 and payload_ <= 127) or payload_ == 9);
}

std::string AudioCodec::getMimeType() const
{
    return "audio";
}

std::string AudioCodec::getMimeSubtype() const
{
    return codecName_;
}

const ost::PayloadFormat& AudioCodec::getPayloadFormat()
{
    return *payloadFormat_;
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

uint8 AudioCodec::getChannel() const
{
    return channel_;
}

double AudioCodec::getBitRate() const
{
    return bitrate_;
}

AudioCodec::~AudioCodec()
{
    delete payloadFormat_;
}

} // end namespace sfl
