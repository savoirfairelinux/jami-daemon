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

namespace sfl {

AudioCodec::AudioCodec(uint8 payload, const std::string &codecName,
                       int clockRate, int frameSize, int channel) :
    codecName_(codecName),
    clockRate_(clockRate),
    channel_(channel),
    frameSize_(frameSize),
    bitrate_(0.0),
    bandwidth_(0.0),
    payload_(payload),
    hasDynamicPayload_((payload_ >= 96 and payload_ <= 127) or payload_ == 9)
{}

AudioCodec::AudioCodec(const AudioCodec& c) :
    codecName_(c.codecName_),
    clockRate_(c.clockRate_),
    channel_(c.channel_),
    frameSize_(c.frameSize_),
    bitrate_(c.bitrate_),
    bandwidth_(c.bandwidth_),
    payload_(c.payload_),
    hasDynamicPayload_(c.hasDynamicPayload_)
{}

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

} // end namespace sfl
