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
#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <assert.h>
#include <cc++/thread.h>
#include "logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}



static std::map<std::string, std::string> encoders;
static std::vector<std::string> video_codecs;
/* application wide mutex to protect concurrent access to avcodec */
static ost::Mutex avcodec_lock;

namespace {

void findInstalledVideoCodecs()
{
	std::vector<std::string> libav_codecs;
    AVCodec *p = NULL;
    while ((p = av_codec_next(p)))
        if (p->type == AVMEDIA_TYPE_VIDEO)
            libav_codecs.push_back(p->name);

	std::map<std::string, std::string>::const_iterator it;
    for (it = encoders.begin(); it != encoders.end(); ++it) {
        if (std::find(libav_codecs.begin(), libav_codecs.end(), it->second) != libav_codecs.end())
        	video_codecs.push_back(it->first);
        else
        	ERROR("Didn't find \"%s\" encoder\n", it->second.c_str());
    }
}


} // end anon namespace

namespace libav_utils {


std::vector<std::string> getVideoCodecList()
{
	return video_codecs;
}

static int avcodecManageMutex(void **mutex, enum AVLockOp op)
{
    (void)mutex; // no need to store our mutex in ffmpeg
    switch(op) {
    case AV_LOCK_CREATE:
        break; // our mutex is already created
    case AV_LOCK_DESTROY:
        break; // our mutex doesn't need to be destroyed
    case AV_LOCK_OBTAIN:
        avcodec_lock.enter();
        break;
    case AV_LOCK_RELEASE:
        avcodec_lock.leave();
        break;
    }

    return 0;
}

std::map<std::string, std::string> encodersMap()
{
	return encoders;
}

void sfl_avcodec_init()
{
    static int done = 0;
    ost::MutexLock lock(avcodec_lock);
    if (done)
        return;

    done = 1;

    av_register_all();
    avdevice_register_all();

    av_lockmgr_register(avcodecManageMutex);

    /* list of codecs tested and confirmed to work */
    encoders["H264"] 		    = "libx264";
    encoders["H263-2000"]		= "h263p";
    encoders["VP8"]			    = "libvpx";
    encoders["MP4V-ES"]         = "mpeg4";


    //FFmpeg needs to be modified to allow us to send configuration
    //inline, with CODEC_FLAG_GLOBAL_HEADER
    //encoders["THEORA"]		= "libtheora";

    // ffmpeg hardcodes RTP output format to H263-2000
    // but it can receive H263-1998
    //encoders["H263-1998"]		= "h263p";

    // ffmpeg doesn't know RTP format for H263 (payload type = 34)
    //encoders["H263"]          = "h263";

    findInstalledVideoCodecs();
}

} // end namespace libav_utils
