/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *
 * This file is free software: you can redistribute it and*or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Sropulpof is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Sropulpof.  If not, see <http:*www.gnu.org*licenses*>.
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

#include "sdpmedia.h"
#include <string.h>
#include <sstream>
#include <iostream>
#include "Codec.h"

static const char* streamDirectionStr[DIR_COUNT] = {
    "sendrecv",
    "sendonly",
    "recvonly",
    "inactive"
};

static const char* mediaTypeStr[MEDIA_COUNT] = {
    "audio",
    "video",
    "application",
    "text",
    "image",
    "message"
};

sdpMedia::sdpMedia (int type)
    : _media_type ( (mediaType) type), _codec_list (0), port (0), _stream_type (SEND_RECEIVE) {}


sdpMedia::sdpMedia (std::string type, int port, std::string dir)
    : _media_type ( (mediaType)-1), _codec_list (0), port (port),
      _stream_type ( (streamDirection)-1)
{
    unsigned int i;

    for (i=0 ; i<MEDIA_COUNT ; i++)
        if (!strcmp (type.c_str(), mediaTypeStr[i])) {
            _media_type = (mediaType) i;
            break;
        }

    if (!strcmp (dir.c_str(), "default"))
        dir = DEFAULT_STREAM_DIRECTION;

    for (i=0; i<DIR_COUNT; i++)
        if (!strcmp (dir.c_str(), streamDirectionStr[i])) {
            _stream_type = (streamDirection) i;
            break;
        }
}


sdpMedia::~sdpMedia()
{
    _codec_list.clear();
}

void sdpMedia::add_codec (sfl::Codec* codec)
{
    _codec_list.push_back (codec);
}

const char *sdpMedia::get_stream_direction_str (void) const
{
    if (_stream_type < 0 || _stream_type >= DIR_COUNT)
        return "unknown";

    return streamDirectionStr[ _stream_type ];
}
