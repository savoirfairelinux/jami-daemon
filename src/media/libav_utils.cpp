/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
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
#include <ciso646> // fix windows compiler bug

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

static constexpr const char* AVLOGLEVEL = "AVLOGLEVEL";

static void
setAvLogLevel()
{
#ifndef RING_UWP
    char* envvar = getenv(AVLOGLEVEL);
    signed level = AV_LOG_WARNING;

    if (envvar != nullptr) {
        if (not (std::istringstream(envvar) >> level))
            level = AV_LOG_ERROR;

        level = std::max(AV_LOG_QUIET, std::min(level, AV_LOG_DEBUG));
    }
    av_log_set_level(level);
#else
    av_log_set_level(0);
#endif
}

#ifdef __ANDROID__
static void
androidAvLogCb(void* ptr, int level, const char* fmt, va_list vl)
{
    if (level > av_log_get_level())
        return;

    char line[1024];
    int print_prefix = 1;
    int android_level;
    va_list vl2;
    va_copy(vl2, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);

    // replace unprintable characters with '?'
    int idx = 0;
    while (line[idx]) {
        if (line[idx] < 0x08 || (line[idx] > 0x0D && line[idx] < 0x20))
            line[idx] = '?';
        ++idx;
    }

    switch(level) {
        case AV_LOG_QUIET:   android_level = ANDROID_LOG_SILENT;  break;
        case AV_LOG_PANIC:   android_level = ANDROID_LOG_FATAL;   break;
        case AV_LOG_FATAL:   android_level = ANDROID_LOG_FATAL;   break;
        case AV_LOG_ERROR:   android_level = ANDROID_LOG_ERROR;   break;
        case AV_LOG_WARNING: android_level = ANDROID_LOG_WARN;    break;
        case AV_LOG_INFO:    android_level = ANDROID_LOG_INFO;    break;
        case AV_LOG_VERBOSE: android_level = ANDROID_LOG_INFO;    break;
        case AV_LOG_DEBUG:   android_level = ANDROID_LOG_DEBUG;   break;
        case AV_LOG_TRACE:   android_level = ANDROID_LOG_VERBOSE; break;
        default:             android_level = ANDROID_LOG_DEFAULT; break;
    }
    __android_log_print(android_level, "FFmpeg", line);
}
#endif

static void
init_once()
{
    av_register_all();
    avdevice_register_all();
    avformat_network_init();

    av_lockmgr_register(avcodecManageMutex);

    if (getDebugMode())
        setAvLogLevel();

#ifdef __ANDROID__
    // android doesn't like stdout and stderr :(
    av_log_set_callback(androidAvLogCb);
#endif
}

static std::once_flag already_called;

void ring_avcodec_init()
{
    std::call_once(already_called, init_once);
}


int libav_pixel_format(int fmt)
{
    switch (fmt) {
        case video::VIDEO_PIXFMT_BGRA: return AV_PIX_FMT_BGRA;
        case video::VIDEO_PIXFMT_RGBA: return AV_PIX_FMT_RGBA;
        case video::VIDEO_PIXFMT_YUYV422: return AV_PIX_FMT_YUYV422;
        case video::VIDEO_PIXFMT_YUV420P: return AV_PIX_FMT_YUV420P;
        case video::VIDEO_PIXFMT_NV21: return AV_PIX_FMT_NV21;
    }
    return fmt;
}

int ring_pixel_format(int fmt)
{
    switch (fmt) {
        case AV_PIX_FMT_YUYV422: return video::VIDEO_PIXFMT_YUYV422;
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

bool
is_yuv_planar(const AVPixFmtDescriptor& desc)
{
    if (not (desc.flags & AV_PIX_FMT_FLAG_PLANAR) or desc.flags & AV_PIX_FMT_FLAG_RGB)
        return false;

    /* handle formats that do not use all planes */
    unsigned used_bit_mask = (1u << desc.nb_components) - 1;
    for (unsigned i = 0; i < desc.nb_components; ++i)
        used_bit_mask &= ~(1u << desc.comp[i].plane);

    return not used_bit_mask;
}

}} // namespace ring::libav_utils
