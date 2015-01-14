/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Luca Barbato <lu_zero@gentoo.org>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#include "libav_deps.h"
#include "video/video_base.h"
#include "logger.h"

#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <exception>

std::map<std::string, std::string> encoders_;
std::vector<std::string> installed_video_codecs_;

static void
findInstalledVideoCodecs()
{
    std::vector<std::string> libav_codecs;
    AVCodec *p = NULL;
    while ((p = av_codec_next(p)))
        if (p->type == AVMEDIA_TYPE_VIDEO)
            libav_codecs.push_back(p->name);

    for (const auto &it : encoders_) {
        if (std::find(libav_codecs.begin(), libav_codecs.end(), it.second) != libav_codecs.end())
            installed_video_codecs_.push_back(it.first);
        else
            RING_ERR("Didn't find \"%s\" encoder", it.second.c_str());
    }
}

namespace libav_utils {

std::vector<std::string> getVideoCodecList()
{
    if (installed_video_codecs_.empty())
        findInstalledVideoCodecs();
    return installed_video_codecs_;
}

// protect libav/ffmpeg access
static int
avcodecManageMutex(void **data, enum AVLockOp op)
{
    auto mutex = reinterpret_cast<std::mutex**>(data);
    int ret = 0;
    switch (op) {
        case AV_LOCK_CREATE:
            try {
                *mutex = new std::mutex;
            } catch (const std::bad_alloc& e) {
                return AVERROR(ENOMEM);
            }
            break;
        case AV_LOCK_OBTAIN:
            (*mutex)->lock();
            break;
        case AV_LOCK_RELEASE:
            (*mutex)->unlock();
            break;
        case AV_LOCK_DESTROY:
            delete *mutex;
            *mutex = nullptr;
            break;
        default:
#ifdef AVERROR_BUG
            return AVERROR_BUG;
#else
            break;
#endif
    }
    return AVERROR(ret);
}

std::map<std::string, std::string> encodersMap()
{
    return encoders_;
}

static void init_once()
{
    av_register_all();
    avdevice_register_all();
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53, 13, 0)
    avformat_network_init();
#endif

    av_lockmgr_register(avcodecManageMutex);

    if (getDebugMode())
        av_log_set_level(AV_LOG_VERBOSE);

    /* list of codecs tested and confirmed to work */
    encoders_["H264"]        = "libx264";
    encoders_["H263-2000"]   = "h263p";
    encoders_["VP8"]         = "libvpx";
    encoders_["MP4V-ES"]     = "mpeg4";

    encoders_["PCMA"]        = "pcm_alaw";
    encoders_["PCMU"]        = "pcm_mulaw";
    encoders_["opus"]        = "libopus";
    encoders_["G722"]        = "g722";
    encoders_["speex"]       = "libspeex";

    //FFmpeg needs to be modified to allow us to send configuration
    //inline, with CODEC_FLAG_GLOBAL_HEADER
    //encoders["THEORA"]        = "libtheora";

    // ffmpeg hardcodes RTP output format to H263-2000
    // but it can receive H263-1998
    // encoders["H263-1998"]        = "h263p";

    // ffmpeg doesn't know RTP format for H263 (payload type = 34)
    //encoders["H263"]          = "h263";

    findInstalledVideoCodecs();
}

static std::once_flag already_called;

void sfl_avcodec_init()
{
    std::call_once(already_called, init_once);
}

std::vector<std::map<std::string, std::string> >
getDefaultCodecs()
{
    const char * const DEFAULT_BITRATE = "400";
    sfl_avcodec_init();
    std::vector<std::map<std::string, std::string> > result;
    for (const auto &item : installed_video_codecs_) {
        std::map<std::string, std::string> codec;
        // FIXME: get these keys from proper place
        codec["name"] = item;
        codec["bitrate"] = DEFAULT_BITRATE;
        codec["enabled"] = "true";
        // FIXME: make a nicer version of this
        if (item == "H264")
            codec["parameters"] = DEFAULT_H264_PROFILE_LEVEL_ID;
        result.push_back(codec);
    }
    return result;
}

int libav_pixel_format(int fmt)
{
    switch (fmt) {
        case VIDEO_PIXFMT_BGRA: return PIXEL_FORMAT(BGRA);
        case VIDEO_PIXFMT_YUV420P: return PIXEL_FORMAT(YUV420P);
    }
    return fmt;
}

int sfl_pixel_format(int fmt)
{
    switch (fmt) {
        case PIXEL_FORMAT(YUV420P): return VIDEO_PIXFMT_YUV420P;
    }
    return fmt;
}

void sfl_url_split(const char *url,
                   char *hostname, size_t hostname_size, int *port,
                   char *path, size_t path_size)
{
    av_url_split(NULL, 0, NULL, 0, hostname, hostname_size, port,
                 path, path_size, url);
}

} // end namespace libav_utils
