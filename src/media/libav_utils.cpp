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

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "config.h"
#include "video/video_base.h"
#include "logger.h"

#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <exception>

namespace ring { namespace libav_utils {

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
}

static std::once_flag already_called;

void ring_avcodec_init()
{
    std::call_once(already_called, init_once);
}


int libav_pixel_format(int fmt)
{
    switch (fmt) {
        case video::VIDEO_PIXFMT_BGRA: return PIXEL_FORMAT(BGRA);
        case video::VIDEO_PIXFMT_RGBA: return PIXEL_FORMAT(RGBA);
        case video::VIDEO_PIXFMT_YUV420P: return PIXEL_FORMAT(YUV420P);
    }
    return fmt;
}

int ring_pixel_format(int fmt)
{
    switch (fmt) {
        case PIXEL_FORMAT(YUV420P): return video::VIDEO_PIXFMT_YUV420P;
    }
    return fmt;
}

void ring_url_split(const char *url,
                   char *hostname, size_t hostname_size, int *port,
                   char *path, size_t path_size)
{
    av_url_split(NULL, 0, NULL, 0, hostname, hostname_size, port,
                 path, path_size, url);
}

}} // namespace ring::libav_utils
