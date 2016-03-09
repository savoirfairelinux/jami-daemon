/*
 *  Copyright (C) 2013-2016 Savoir-faire Linux Inc.
 *
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

#ifndef __LIBAV_DEPS_H__
#define __LIBAV_DEPS_H__

/* LIBAVFORMAT_VERSION_CHECK checks for the right version of libav and FFmpeg
 * a is the major version
 * b and c the minor and micro versions of libav
 * d and e the minor and micro versions of FFmpeg */
#define LIBAVFORMAT_VERSION_CHECK( a, b, c, d, e ) \
    ( (LIBAVFORMAT_VERSION_MICRO <  100 && LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT( a, b, c ) ) || \
      (LIBAVFORMAT_VERSION_MICRO >= 100 && LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT( a, d, e ) ) )

/* LIBAVCODEC_VERSION_CHECK checks for the right version of libav and FFmpeg
 * a is the major version
 * b and c the minor and micro versions of libav
 * d and e the minor and micro versions of FFmpeg */
#define LIBAVCODEC_VERSION_CHECK( a, b, c, d, e ) \
    ( (LIBAVCODEC_VERSION_MICRO <  100 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( a, b, c ) ) || \
      (LIBAVCODEC_VERSION_MICRO >= 100 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( a, d, e ) ) )

/* LIBAVUTIL_VERSION_CHECK checks for the right version of libav and FFmpeg
 * a is the major version
 * b and c the minor and micro versions of libav
 * d and e the minor and micro versions of FFmpeg */
#define LIBAVUTIL_VERSION_CHECK( a, b, c, d, e ) \
    ( (LIBAVUTIL_VERSION_MICRO <  100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( a, b, c ) ) || \
      (LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( a, d, e ) ) )

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#if LIBAVUTIL_VERSION_CHECK(51, 33, 0, 60, 100)
#include <libavutil/time.h>
#endif
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h> // for av_rescale_q (old libav support)
#include <libavutil/imgutils.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/log.h>
}

#include "libav_utils.h"

#if !LIBAVFORMAT_VERSION_CHECK(54,20,3,59,103)
#error "Used libavformat doesn't support sdp custom_io"
#endif

#if !LIBAVUTIL_VERSION_CHECK(51, 42, 0, 74, 100) && !defined(FF_API_PIX_FMT)
#define AVPixelFormat PixelFormat
#define PIXEL_FORMAT(FMT) PIX_FMT_ ## FMT

static inline const AVPixFmtDescriptor *av_pix_fmt_desc_get(enum AVPixelFormat pix_fmt)
{
    if (pix_fmt < 0 || pix_fmt >= PIX_FMT_NB)
        return NULL;
    return &av_pix_fmt_descriptors[pix_fmt];
}

#else
#define PIXEL_FORMAT(FMT) AV_PIX_FMT_ ## FMT
#endif

#if !LIBAVCODEC_VERSION_CHECK(54, 28, 0, 59, 100)
#define avcodec_free_frame(x) av_freep(x)
#endif

// Especially for Fedora < 20 and UBUNTU < 14.10
#define USE_OLD_AVU ! LIBAVUTIL_VERSION_CHECK(52, 8, 0, 19, 100)

#if USE_OLD_AVU
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#define av_frame_unref avcodec_get_frame_defaults
#define av_frame_get_buffer(x, y) avpicture_alloc((AVPicture *)(x), \
                                                  (AVPixelFormat)(x)->format, \
                                                  (x)->width, (x)->height)
#endif


#endif // __LIBAV_DEPS_H__
