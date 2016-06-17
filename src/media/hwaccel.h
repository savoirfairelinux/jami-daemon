/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

 /* File borrowed and adapted from FFmpeg*/

#ifndef RING_HWACCEL
#define RING_HWACCEL

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "config.h"

enum HWAccelID {
    HWACCEL_NONE = 0,
    HWACCEL_AUTO,
    HWACCEL_VDPAU,
    HWACCEL_DXVA2,
    HWACCEL_VDA,
    HWACCEL_VIDEOTOOLBOX,
    HWACCEL_QSV,
    HWACCEL_VAAPI,
    HWACCEL_CUVID,
};

typedef struct HWAccel {
    const char *name;
    int (*init)(AVCodecContext *s);
    enum HWAccelID id;
    enum AVPixelFormat pix_fmt;
} HWAccel;

typedef struct RingHWContext {
    AVCodecContext *dec_ctx;
    AVCodec *dec;

    /* hwaccel options */
    enum HWAccelID hwaccel_id;
    char *hwaccel_device;
    enum AVPixelFormat hwaccel_output_format;

    /* hwaccel context */
    void  *hwaccel_ctx;
    void (*hwaccel_uninit)(AVCodecContext *s);
    int  (*hwaccel_get_buffer)(AVCodecContext *s, AVFrame *frame, int flags);
    int  (*hwaccel_retrieve_data)(AVCodecContext *s, AVFrame *frame);
    enum AVPixelFormat hwaccel_pix_fmt;
    enum AVPixelFormat hwaccel_retrieved_pix_fmt;
    AVBufferRef *hw_frames_ctx;
} RingHWContext;

int vaapi_decode_init(AVCodecContext *avctx);

// uncomment as needed
const HWAccel hwaccels[] = {
    { "vaapi", vaapi_decode_init, HWACCEL_VAAPI, AV_PIX_FMT_VAAPI },
    { 0 },
};

enum AVPixelFormat get_format(AVCodecContext *s, const enum AVPixelFormat *pix_fmts);
int get_buffer(AVCodecContext *s, AVFrame *frame, int flags);

#endif /* RING_HWACCEL */
