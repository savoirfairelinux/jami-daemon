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

namespace ring { namespace video {



HardwareAccelContext::HardwareAccelContext()
{
}

HardwareAccelContext::~HardwareAccelContext()
{
}

void
HardwareAccelContext::findHardwareAccel(AVCodecContext* codecCtx, std::string hardwareAccelName)
{
    // check if hardwareAccelName is a legal value:
    // null, empty, "none", "auto", key in hardwareAccelMap_
    // set callbacks and flag for automatic detection
    if ((!hardwareAccelName || !hardwareAccelName[0] || hardwareAccelName == "auto") {
        autoDetect_ = 1;

    if (autoDetect_ || hardwareAccelMap_.find(hardwareAccelName) != hardwareAccelMap_.end()) {
        codecCtx->opaque = this;
        codecCtx->get_format = getFormatCb;
        codecCtx->get_buffer2 = getBufferCb;
        codecCtx->thread_safe_callbacks = 1;
        codecCtx_ = codecCtx;
    } else if (hardwareAccelName != "none")
        RING_ERR("Unrecognized hardware acceleration '%s'; falling back to software decoding",
                 hardwareAccelName);
}

enum AVPixelFormat
HardwareAccelContext::getFormat(const enum AVPixelFormat* pixFmts)
{
    // get HardwareAccel object from pixel format and initialize it
    // if autoDetect_, get first available accel in map



    // static const HWAccel *get_hwaccel(enum AVPixelFormat pix_fmt)
    // {
    //     int i;
    //     for (i = 0; hwaccels[i].name; i++)
    //         if (hwaccels[i].pix_fmt == pix_fmt)
    //             return &hwaccels[i];
    //     return NULL;
    // }

    // RingHWContext *rhw = static_cast<RingHWContext*>(s->opaque);
    // const enum AVPixelFormat *p;
    // int ret;
    //
    // for (p = pix_fmts; *p != -1; p++) {
    //     const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);
    //     const HWAccel *hwaccel;
    //
    //     if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
    //         break;
    //
    //     hwaccel = get_hwaccel(*p);
    //     if (!hwaccel ||
    //         (rhw->hwaccel_id != HWACCEL_AUTO && rhw->hwaccel_id != hwaccel->id))
    //         continue;
    //
    //     if (rhw->hwaccel_id == HWACCEL_AUTO)
    //         RING_DBG("Automatically detected hwaccel %s, attempting to initialize...",
    //                   hwaccel->name);
    //
    //     ret = hwaccel->init(s);
    //     if (ret < 0) {
    //         if (rhw->hwaccel_id == hwaccel->id) {
    //             RING_ERR("%s hwaccel requested for an input stream that cannot be initialized",
    //                      hwaccel->name);
    //             return AV_PIX_FMT_NONE;
    //         }
    //         continue;
    //     }
    //     rhw->hwaccel_pix_fmt = *p;
    //     break;
    // }
    //
    // return *p;
}

int
HardwareAccelContext::getBuffer(AVFrame* frame, int flags)
{
    if (frame->format == pixFmt_)
        return hardwareAccel_->getBuffer(ctx, frame, flags);

    return avcodec_default_get_buffer2(ctx, frame, flags);
}

}} // namespace ring::video
