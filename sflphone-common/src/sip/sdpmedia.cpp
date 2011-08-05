/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

sdpMedia::sdpMedia (int type)
    : _media_type ( (mediaType) type), _audio_codec_list (0), _video_codec_list (0), port (0) {}


sdpMedia::sdpMedia (std::string type, int port)
    : _media_type ( (mediaType)-1), _audio_codec_list (0), _video_codec_list (0), port (port)
{
	static const char* mediaTypeStr[MEDIA_COUNT] = {
	    "audio",
	    "video",
	    "application",
	    "text",
	    "image",
	    "message"
	};

    unsigned int i;
    for (i=0 ; i<MEDIA_COUNT ; i++)
        if (type == mediaTypeStr[i]) {
            _media_type = (mediaType) i;
            break;
        }
}

void sdpMedia::add_codec (sfl::Codec* codec)
{
    _audio_codec_list.push_back (codec);
}

void sdpMedia::add_codec (std::string codec)
{
    _video_codec_list.push_back (codec);
}
