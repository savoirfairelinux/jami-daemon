/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include "libav_utils.h"
#include <list>
#include <algorithm>
#include <string>
#include <iostream>

extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace libav_utils {

bool isSupportedCodec(const char *name)
{
    static std::list<std::string> SUPPORTED_CODECS;
    if (SUPPORTED_CODECS.empty())
    {
        SUPPORTED_CODECS.push_back("mpeg4");
        SUPPORTED_CODECS.push_back("h263p");
        SUPPORTED_CODECS.push_back("libx264");
        SUPPORTED_CODECS.push_back("libtheora");
        SUPPORTED_CODECS.push_back("libvpx");
    }

    return std::find(SUPPORTED_CODECS.begin(), SUPPORTED_CODECS.end(), name) !=
        SUPPORTED_CODECS.end();
}



std::list<std::string> installedCodecs()
{
    // FIXME: not thread safe
    static bool registered = false;
    if (not registered)
    {
        av_register_all();
        registered = true;
    }

    std::list<std::string> codecs;
    AVCodec *p = NULL, *p2;
    const char *last_name = "000";
    while (true)
    {
        p2 = NULL;
        while ((p = av_codec_next(p)))
        {
            if((p2 == NULL or strcmp(p->name, p2->name) < 0) and
                strcmp(p->name, last_name) > 0)
            {
                p2 = p;
            }
        }
        if (p2 == NULL)
            break;
        last_name = p2->name;

        switch(p2->type) 
        {
            case AVMEDIA_TYPE_VIDEO:
                if (isSupportedCodec(p2->name))
                    codecs.push_back(p2->name);
                break;
            default:
                break;
        }
    }
    return codecs;
}
} // end namespace libav_utils
