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

#include "hwaccel.h"

#include "logger.h"

static const HWAccel *get_hwaccel(enum AVPixelFormat pix_fmt)
{
    int i;
    for (i = 0; hwaccels[i].name; i++)
        if (hwaccels[i].pix_fmt == pix_fmt)
            return &hwaccels[i];
    return NULL;
}

static enum AVPixelFormat get_format(AVCodecContext *s, const enum AVPixelFormat *pix_fmts)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(s->opaque);
    const enum AVPixelFormat *p;
    int ret;

    for (p = pix_fmts; *p != -1; p++) {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);
        const HWAccel *hwaccel;

        if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
            break;

        hwaccel = get_hwaccel(*p);
        if (!hwaccel || (!rhw->auto_detect && rhw->hwaccel_id != hwaccel->id))
            continue;

        if (rhw->auto_detect)
            RING_DBG("Automatically detected hwaccel %s, attempting to initialize...",
                      hwaccel->name);

        ret = hwaccel->init(s);
        if (ret < 0) {
            if (rhw->hwaccel_id == hwaccel->id) {
                RING_ERR("%s hwaccel requested for an input stream that cannot be initialized",
                         hwaccel->name);
                return AV_PIX_FMT_NONE;
            }
            continue;
        }
        rhw->hwaccel_id = hwaccel->id;
        rhw->hwaccel_pix_fmt = *p;
        break;
    }

    return *p;
}

static int get_buffer(AVCodecContext *s, AVFrame *frame, int flags)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(s->opaque);

    if (rhw->hwaccel_get_buffer && frame->format == rhw->hwaccel_pix_fmt)
        return rhw->hwaccel_get_buffer(s, frame, flags);

    return avcodec_default_get_buffer2(s, frame, flags);
}

int find_hwaccel(AVCodecContext *avctx, const char* requested_hwaccel) {
    RingHWContext *rhw = static_cast<RingHWContext*>(av_mallocz(sizeof(*rhw)));
    if (!rhw) {
        RING_ERR("Hardware acceleration context is NULL");
        return -1;
    }

    rhw->auto_detect = 0;
    const char *hwaccel_name = requested_hwaccel;
    if (!hwaccel_name || !hwaccel_name[0] || !strcmp(hwaccel_name, "auto")) {
        rhw->auto_detect = 1;
        rhw->hwaccel_id = HWACCEL_NONE; // will be chosen in get_format callback
    }
    else if (!strcmp(hwaccel_name, "none"))
        rhw->hwaccel_id = HWACCEL_NONE;
    else {
        int i;
        for (i = 0; hwaccels[i].name; i++) {
            if (!strcmp(hwaccels[i].name, hwaccel_name)) {
                RING_DBG("Found match for hwaccel: %s", hwaccel_name);
                rhw->hwaccel_id = hwaccels[i].id;
                break;
            }
        }

        if (!rhw->hwaccel_id) {
            RING_ERR("Unrecognized hardware acceleration: %s", hwaccel_name);
            return -1;
        }
    }
    rhw->hwaccel_pix_fmt = AV_PIX_FMT_NONE;

    avctx->opaque = rhw; // store for later
    avctx->get_format = get_format;
    avctx->get_buffer2 = get_buffer;
    avctx->thread_safe_callbacks = 1;
    rhw->dec_ctx = avctx;

    return 0;
}
