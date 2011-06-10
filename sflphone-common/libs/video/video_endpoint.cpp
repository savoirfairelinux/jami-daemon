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

#include "video_endpoint.h"
#include <iostream>
#include <sstream>
#include <map>

namespace sfl_video {

/* anonymous namespace */
namespace {
std::string encoderName(int payload)
{
    std::string result = getCodecsMap()[payload];
    if (result.empty())
        return "MISSING";
    else
        return result;
}

int FAKE_BITRATE()
{
    return 1000000;
}

int getBitRate(int payload)
{
    return FAKE_BITRATE();
}

int getBandwidthPerCall(int payload)
{
    return FAKE_BITRATE();
}
} // end anonymous namespace

std::map<int, std::string> getCodecsMap()
{
    static std::map<int, std::string> CODECS_MAP;
    if (CODECS_MAP.empty())
    {
        CODECS_MAP[96] = "H263-2000";
        CODECS_MAP[97] = "H264";
        CODECS_MAP[98] = "MP4V-ES";
        CODECS_MAP[99] = "VP8";
        CODECS_MAP[100] = "THEORA";
    }
    return CODECS_MAP;
}

std::vector<std::string> getCodecSpecifications(int payload)
{
    std::vector<std::string> v;
    std::stringstream ss;

    // Add the name of the codec
    v.push_back (encoderName(payload));

    // Add the bit rate
    ss << getBitRate(payload);
    v.push_back(ss.str());
    ss.str("");

    // Add the bandwidth information
    ss << getBandwidthPerCall(payload);
    v.push_back (ss.str());
    ss.str ("");

    return v;
}

} // end namespace sfl_video
