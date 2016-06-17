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

#include "libav_deps.h" // MUST BE INCLUDED FIRST
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

enum AVPixelFormat get_format(AVCodecContext *s, const enum AVPixelFormat *pix_fmts)
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
        if (!hwaccel ||
            (rhw->hwaccel_id != HWACCEL_AUTO && rhw->hwaccel_id != hwaccel->id))
            continue;

        ret = hwaccel->init(s);
        if (ret < 0) {
            if (rhw->hwaccel_id == hwaccel->id) {
                RING_ERR("%s hwaccel requested for an input stream that cannot be initialized",
                         hwaccel->name);
                return AV_PIX_FMT_NONE;
            }
            continue;
        }
        rhw->hwaccel_pix_fmt = *p;
        break;
    }

    return *p;
}

int get_buffer(AVCodecContext *s, AVFrame *frame, int flags)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(s->opaque);

    if (rhw->hwaccel_get_buffer && frame->format == rhw->hwaccel_pix_fmt)
        return rhw->hwaccel_get_buffer(s, frame, flags);

    return avcodec_default_get_buffer2(s, frame, flags);
}
