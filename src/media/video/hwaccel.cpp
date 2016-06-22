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

#if defined(HAVE_VAAPI_DRM) || defined(HAVE_VAAPI_X11)
#include "v4l2/vaapi.h"
#endif

#include "logger.h"

namespace ring { namespace video {

const HardwareAccelProxy::AccelInfo accels[] = {
#if defined(HAVE_VAAPI_DRM) || defined(HAVE_VAAPI_X11)
    { "vaapi", AV_PIX_FMT_VAAPI, HardwareAccelProxy::make<VaapiAccel> },
#endif
    { 0 }
};

void
HardwareAccelProxy::findHardwareAccel(AVCodecContext* codecCtx, std::string hardwareAccelName)
{
    // it's too early to initialize the hardware acceleration,
    // but we need to figure out if we can use it
    int foundAccel = 0;
    if (hardwareAccelName.empty() || hardwareAccelName == "auto")
        autoDetect_ = 1;
    else if (hardwareAccelName != "none") {
        int count = sizeof(accels) / sizeof(AccelInfo);
        for (int i = 0; accels[i].name; i++) {
            AccelInfo accel = accels[i];
            if (accel.name == hardwareAccelName) {
                pixFmt_ = accel.format;
                foundAccel = 1;
                break;
            }
        }
    }

    if (autoDetect_ || foundAccel) {
        codecCtx->opaque = this;
        codecCtx->get_format = getFormatCb;
        codecCtx->get_buffer2 = getBufferCb;
        codecCtx->thread_safe_callbacks = 1;
        // hwaccel frame decoding is unstable, and may cost performance
        // http://forum.doom9.org/showthread.php?p=1684864#post1684864
        codecCtx->thread_count = 1;
    } else if (hardwareAccelName != "none")
        RING_ERR("Unrecognized hardware acceleration '%s'; falling back to software decoding",
                hardwareAccelName.c_str());
}

enum AVPixelFormat
HardwareAccelProxy::getFormat(AVCodecContext* codecCtx, const enum AVPixelFormat* pixFmts)
{
    AccelCreator makeAccel = NULL;
    const AVPixelFormat* p;
    for (p = pixFmts; *p != AV_PIX_FMT_NONE; p++) {
        const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(*p);
        if (!desc || !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
            break;

        AVPixelFormat toCompare = autoDetect_ ? *p : pixFmt_;
        for (const auto& accel : accels) {
            if (accel.format == toCompare) {
                makeAccel = accel.makeFunc;
                break;
            }
        }

        if (makeAccel) {
            hardwareAccel_ = makeAccel();
            hardwareAccel_->init(codecCtx);
            break;
        }
    }

    return *p;

    // AVPixelFormat backup = *pixFmts;
    // HardwareAccelCreator makeFunc;
    // if (autoDetect_) {
    //     const AVPixelFormat* p;
    //     for (p = pixFmts; *p != AV_PIX_FMT_NONE; p++) {
    //         const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(*p);
    //
    //         if (!desc || !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
    //             break;
    //
    //         if (hardwareAccelMap_.find(*p) == hardwareAccelMap_.end())
    //             continue;
    //
    //         RING_DBG("Found match for pixel format %s", av_get_pix_fmt_name(*p));
    //         pixFmt_ = *p;
    //         makeFunc = hardwareAccelMap_[pixFmt_];
    //     }
    // } else if (pixFmt_ != AV_PIX_FMT_NONE) {
    //      makeFunc = hardwareAccelMap_[pixFmt_];
    // }
    //
    // if (!makeFunc)
    //     return avcodec_default_get_format(codecCtx_, &backup);
    //
    // hardwareAccel_ = makeFunc();
    // hardwareAccel_->init(codecCtx_);
    // return pixFmt_;
}

int
HardwareAccelProxy::getBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags)
{
    if (frame->format == pixFmt_)
        return hardwareAccel_->getBuffer(codecCtx, frame, flags);
    else
        return avcodec_default_get_buffer2(codecCtx, frame, flags);
}

int
HardwareAccelProxy::retrieveData(AVCodecContext* codecCtx, AVFrame* frame)
{
    if (frame->format == pixFmt_)
        return hardwareAccel_->retrieveData(codecCtx, frame);
}

}} // namespace ring::video

// static const HWAccel *get_hwaccel(enum AVPixelFormat pix_fmt)
// {
//     int i;
//     for (i = 0; hwaccels[i].name; i++)
//         if (hwaccels[i].pix_fmt == pix_fmt)
//             return &hwaccels[i];
//     return NULL;
// }
//
// static enum AVPixelFormat get_format(AVCodecContext *s, const enum AVPixelFormat *pix_fmts)
// {
//     RingHWContext *rhw = static_cast<RingHWContext*>(s->opaque);
//     const enum AVPixelFormat *p;
//     int ret;
//
//     for (p = pix_fmts; *p != -1; p++) {
//         const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);
//         const HWAccel *hwaccel;
//
//         if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
//             break;
//
//         hwaccel = get_hwaccel(*p);
//         if (!hwaccel ||
//             (rhw->hwaccel_id != HWACCEL_AUTO && rhw->hwaccel_id != hwaccel->id))
//             continue;
//
//         if (rhw->hwaccel_id == HWACCEL_AUTO)
//             RING_DBG("Automatically detected hwaccel %s, attempting to initialize...",
//                       hwaccel->name);
//
//         ret = hwaccel->init(s);
//         if (ret < 0) {
//             if (rhw->hwaccel_id == hwaccel->id) {
//                 RING_ERR("%s hwaccel requested for an input stream that cannot be initialized",
//                          hwaccel->name);
//                 return AV_PIX_FMT_NONE;
//             }
//             continue;
//         }
//         rhw->hwaccel_pix_fmt = *p;
//         break;
//     }
//
//     return *p;
// }
//
// static int get_buffer(AVCodecContext *s, AVFrame *frame, int flags)
// {
//     RingHWContext *rhw = static_cast<RingHWContext*>(s->opaque);
//
//     if (rhw->hwaccel_get_buffer && frame->format == rhw->hwaccel_pix_fmt)
//         return rhw->hwaccel_get_buffer(s, frame, flags);
//
//     return avcodec_default_get_buffer2(s, frame, flags);
// }
//
// int find_hwaccel(AVCodecContext *avctx, const char* requested_hwaccel) {
//     RingHWContext *rhw = static_cast<RingHWContext*>(av_mallocz(sizeof(*rhw)));
//     if (!rhw) {
//         RING_ERR("Hardware acceleration context is NULL");
//         return -1;
//     }
//
//     const char *hwaccel_name = requested_hwaccel;
//     if (!hwaccel_name || !hwaccel_name[0] || !strcmp(hwaccel_name, "auto"))
//         rhw->hwaccel_id = HWACCEL_AUTO;
//     else if (!strcmp(hwaccel_name, "none"))
//         rhw->hwaccel_id = HWACCEL_NONE;
//     else {
//         int i;
//         for (i = 0; hwaccels[i].name; i++) {
//             if (!strcmp(hwaccels[i].name, hwaccel_name)) {
//                 RING_DBG("Found match for hwaccel: %s", hwaccel_name);
//                 rhw->hwaccel_id = hwaccels[i].id;
//                 break;
//             }
//         }
//
//         if (!rhw->hwaccel_id) {
//             RING_ERR("Unrecognized hardware acceleration: %s", hwaccel_name);
//             return -1;
//         }
//     }
//     rhw->hwaccel_pix_fmt = AV_PIX_FMT_NONE;
//
//     avctx->opaque = rhw; // store for later
//     avctx->get_format = get_format;
//     avctx->get_buffer2 = get_buffer;
//     avctx->thread_safe_callbacks = 1;
//     rhw->dec_ctx = avctx;
//
//     return 0;
// }
