/*
 * Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

namespace sfl {

AudioCodec::AudioCodec (uint8 payload, const std::string &codecName) :
        _codecName (codecName), _clockRate (8000), _channel (1), _bitrate (0.0),
        _bandwidth (0), _hasDynamicPayload (false), _payload(payload)
{
    init (payload, _clockRate);
}

AudioCodec::AudioCodec (const AudioCodec& codec) :
        _codecName (codec._codecName), _clockRate (codec._clockRate), _channel (
            codec._channel), _bitrate (codec._bitrate), _bandwidth (
                codec._bandwidth), _hasDynamicPayload (false), _payload(codec._payload)
{
    init (codec._payload, codec._clockRate);
}

void AudioCodec::init (uint8 payloadType, uint32 clockRate)
{
    _payloadFormat = new ost::DynamicPayloadFormat (payloadType, clockRate);

    _hasDynamicPayload = (_payload >= 96 && _payload <= 127) ? true : false;

    // If g722 (payload 9), we need to init libccrtp symetric sessions with using
    // dynamic payload format. This way we get control on rtp clockrate.

    if (_payload == 9) {
        _hasDynamicPayload = true;
    }
}

std::string AudioCodec::getMimeType() const
{
    return "audio";
}

std::string AudioCodec::getMimeSubtype() const
{
    return _codecName;
}

const ost::PayloadFormat& AudioCodec::getPayloadFormat()
{
    return (*_payloadFormat);
}

uint8 AudioCodec::getPayloadType (void) const
{
    return _payload;
}

bool AudioCodec::hasDynamicPayload (void) const
{
    return _hasDynamicPayload;
}

uint32 AudioCodec::getClockRate (void) const
{
    return _clockRate;
}

unsigned AudioCodec::getFrameSize (void) const
{
    return _frameSize;
}

uint8 AudioCodec::getChannel (void) const
{
    return _channel;
}

double AudioCodec::getBitRate (void) const
{
    return _bitrate;
}

double AudioCodec::getBandwidth (void) const
{
    return _bandwidth;
}

} // end namespace sfl
