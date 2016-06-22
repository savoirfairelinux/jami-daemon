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

std::map<std::string, AVPixelFormat> HardwareAccelContext::pixelFormatMap_ = {
    { RING_VAAPI, AV_PIX_FMT_VAAPI }
};

std::map<AVPixelFormat, HardwareAccelContext::HardwareAccelCreator> HardwareAccelContext::hardwareAccelMap_ = {
#if defined(HAVE_VAAPI_DRM) || defined(HAVE_VAAPI_X11)
    { AV_PIX_FMT_VAAPI, make<VaapiAccel> }
#endif
};

void
HardwareAccelContext::findHardwareAccel(AVCodecContext* codecCtx, std::string hardwareAccelName)
{
    // it's too early to initialize the hardware acceleration,
    // but we need to figure out if we can use it
    if ((hardwareAccelName.empty() || hardwareAccelName == "auto"))
        autoDetect_ = 1;

    if (autoDetect_ || pixelFormatMap_.find(hardwareAccelName) != pixelFormatMap_.end()) {
        codecCtx->opaque = this;
        codecCtx->get_format = getFormatCb;
        codecCtx->get_buffer2 = getBufferCb;
        codecCtx->thread_safe_callbacks = 1;
        codecCtx_ = codecCtx;
        if (!autoDetect_)
            pixFmt_ = pixelFormatMap_[hardwareAccelName];
    } else if (hardwareAccelName != "none")
        RING_ERR("Unrecognized hardware acceleration '%s'; falling back to software decoding",
                 hardwareAccelName.c_str());
}

enum AVPixelFormat
HardwareAccelContext::getFormat(const enum AVPixelFormat* pixFmts)
{
    AVPixelFormat backup = *pixFmts;
    HardwareAccelCreator makeFunc;
    if (autoDetect_) {
        const AVPixelFormat* p;
        for (p = pixFmts; *p != AV_PIX_FMT_NONE; p++) {
            const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(*p);

            if (!desc || !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
                break;

            if (hardwareAccelMap_.find(*p) == hardwareAccelMap_.end())
                continue;

            RING_DBG("Found match for pixel format %s", av_get_pix_fmt_name(*p));
            pixFmt_ = *p;
            makeFunc = hardwareAccelMap_[pixFmt_];
        }
    } else if (pixFmt_ != AV_PIX_FMT_NONE) {
         makeFunc = hardwareAccelMap_[pixFmt_];
    }

    if (!makeFunc)
        return avcodec_default_get_format(codecCtx_, &backup);

    hardwareAccel_ = makeFunc();
    hardwareAccel_->init(codecCtx_);
    return pixFmt_;
}

int
HardwareAccelContext::getBuffer(AVFrame* frame, int flags)
{
    if (frame->format == pixFmt_)
        return hardwareAccel_->getBuffer(frame, flags);
    else
        return avcodec_default_get_buffer2(codecCtx_, frame, flags);
}

int
HardwareAccelContext::retrieveData(AVFrame* frame)
{
    if (frame->format == pixFmt_)
        return hardwareAccel_->retrieveData(frame);
}

}} // namespace ring::video
