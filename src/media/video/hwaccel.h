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

#pragma once

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "config.h"

namespace ring { namespace video {

class HardwareAccel {
    public:
        HardwareAccel() {};
        ~HardwareAccel() {};

        virtual void init(AVCodecContext* codecCtx) = 0;
        virtual int getBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags) = 0;
        virtual int retrieveData(AVCodecContext* codecCtx, AVFrame* input) = 0;
};

class HardwareAccelProxy {
    public:
        HardwareAccelProxy() {};
        ~HardwareAccelProxy() {};

        void findHardwareAccel(AVCodecContext* codecCtx, std::string hardwareAccelName);
        int retrieveData(AVCodecContext* codecCtx, AVFrame* frame);

        using AccelCreator = HardwareAccel* (*)();
        template <typename T>
        static HardwareAccel* make() { return new T{}; }

        using AccelInfo = struct AccelInfo {
            const char* name;
            const AVPixelFormat format;
            const AccelCreator makeFunc;
        };

    private:
        HardwareAccel* hardwareAccel_ = nullptr;
        int autoDetect_ = 0;
        AVPixelFormat pixFmt_ = AV_PIX_FMT_NONE;

        enum AVPixelFormat getFormat(AVCodecContext* codecCtx, const enum AVPixelFormat* pixFmts);
        int getBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags);

        static int getBufferCb(AVCodecContext* codecCtx, AVFrame* frame, int flags)
        {
            auto thisPtr = static_cast<HardwareAccelProxy*>(codecCtx->opaque);
            return thisPtr->getBuffer(codecCtx, frame, flags);
        }
        static enum AVPixelFormat getFormatCb(AVCodecContext* codecCtx, const enum AVPixelFormat* pixFmt)
        {
            auto thisPtr = static_cast<HardwareAccelProxy*>(codecCtx->opaque);
            return thisPtr->getFormat(codecCtx, pixFmt);
        }
};

}} // namespace ring::video

// enum HWAccelID {
//     HWACCEL_NONE = 0,
//     HWACCEL_AUTO,
//     HWACCEL_VAAPI,
// };
//
// typedef struct HWAccel {
//     const char *name;
//     int (*init)(AVCodecContext *avctx);
//     enum HWAccelID id;
//     enum AVPixelFormat pix_fmt;
// } HWAccel;
//
// typedef struct RingHWContext {
//     AVCodecContext *dec_ctx;
//     AVCodec *dec;
//
//     /* hwaccel options */
//     enum HWAccelID hwaccel_id;
//     char *hwaccel_device;
//     enum AVPixelFormat hwaccel_output_format;
//
//     /* hwaccel context */
//     void  *hwaccel_ctx;
//     void (*hwaccel_uninit)(AVCodecContext *s);
//     int  (*hwaccel_get_buffer)(AVCodecContext *s, AVFrame *frame, int flags);
//     int  (*hwaccel_retrieve_data)(AVCodecContext *s, AVFrame *frame);
//     enum AVPixelFormat hwaccel_pix_fmt;
//     enum AVPixelFormat hwaccel_retrieved_pix_fmt;
//     AVBufferRef *hw_frames_ctx;
// } RingHWContext;
//
// extern "C" {
// int vaapi_decode_init(AVCodecContext *avctx);
// }
//
// const HWAccel hwaccels[] = {
// #if HAVE_VAAPI_DRM || HAVE_VAAPI_X11
//     { "vaapi", vaapi_decode_init, HWACCEL_VAAPI, AV_PIX_FMT_VAAPI },
// #endif
//     { 0 },
// };
//
// int find_hwaccel(AVCodecContext *avctx, const char* requested_hwaccel);
//
//#endif /* RING_HWACCEL */
