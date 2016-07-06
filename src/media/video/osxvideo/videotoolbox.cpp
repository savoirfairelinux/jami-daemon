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

#include "config.h"

#if USE_HWACCEL

extern "C" {
#if HAVE_VIDEOTOOLBOX
#  include "libavcodec/videotoolbox.h"
#endif
#if HAVE_VDA
#  include "libavcodec/vda.h"
#endif
}

#include "video/hwaccel.h"

#include "logger.h"

typedef struct VTContext {
    AVFrame *tmp_frame;
} VTContext;

static int videotoolbox_retrieve_data(AVCodecContext *avctx, AVFrame *frame)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    VTContext *vt = static_cast<VTContext*>(rhw->hwaccel_ctx);
    CVPixelBufferRef pixbuf = (CVPixelBufferRef)frame->data[3];
    OSType pixel_format = CVPixelBufferGetPixelFormatType(pixbuf);
    CVReturn err;
    uint8_t *data[4] = { 0 };
    int linesize[4] = { 0 };
    int planes, ret, i;
    char codec_str[32];

    av_frame_unref(vt->tmp_frame);

    switch (pixel_format) {
        case kCVPixelFormatType_420YpCbCr8Planar:
            vt->tmp_frame->format = AV_PIX_FMT_YUV420P;
            break;
        case kCVPixelFormatType_422YpCbCr8:
            vt->tmp_frame->format = AV_PIX_FMT_UYVY422;
            break;
        case kCVPixelFormatType_32BGRA:
            vt->tmp_frame->format = AV_PIX_FMT_BGRA;
            break;
#ifdef kCFCoreFoundationVersionNumber10_7
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
            vt->tmp_frame->format = AV_PIX_FMT_NV12;
            break;
#endif
        default:
            av_get_codec_tag_string(codec_str, sizeof(codec_str), avctx->codec_tag);
            RING_ERR("%s: Unsupported pixel format", codec_str);
            return AVERROR(ENOSYS);
    }

    vt->tmp_frame->width = frame->width;
    vt->tmp_frame->height = frame->height;
    ret = av_frame_get_buffer(vt->tmp_frame, 32);
    if (ret < 0)
        return ret;

    err = CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    if (err != kCVReturnSuccess) {
        RING_ERR("Error locking the pixel buffer");
        return AVERROR_UNKNOWN;
    }

    if (CVPixelBufferIsPlanar(pixbuf)) {
        planes = CVPixelBufferGetPlaneCount(pixbuf);
        for (i = 0; i < planes; i++) {
            data[i]     = CVPixelBufferGetBaseAddressOfPlane(pixbuf, i);
            linesize[i] = CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
        }
    } else {
        data[0] = CVPixelBufferGetBaseAddress(pixbuf);
        linesize[0] = CVPixelBufferGetBytesPerRow(pixbuf);
    }

    av_image_copy(vt->tmp_frame->data, vt->tmp_frame->linesize,
                  (const uint8_t **)data, linesize, vt->tmp_frame->format,
                  frame->width, frame->height);

    ret = av_frame_copy_props(vt->tmp_frame, frame);
    CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    if (ret < 0)
        return ret;

    av_frame_unref(frame);
    av_frame_move_ref(frame, vt->tmp_frame);

    return 0;
}

static void videotoolbox_uninit(AVCodecContext *avctx)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    VTContext *vt = static_cast<VTContext*>(rhw->hwaccel_ctx);

    rhw->hwaccel_uninit = NULL;
    rhw->hwaccel_retrieve_data = NULL;

    av_frame_free(&vt->tmp_frame);

    if (rhw->hwaccel_id == HWACCEL_VIDEOTOOLBOX) {
#if CONFIG_VIDEOTOOLBOX
        av_videotoolbox_default_free(avctx);
#endif
    } else {
#if CONFIG_VDA
        av_vda_default_free(avctx);
#endif
    }
    av_freep(&rhw->hwaccel_ctx);
}

int videotoolbox_init(AVCodecContext *avctx)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    int ret = 0;
    VTContext *vt;

    vt = av_mallocz(sizeof(*vt));
    if (!vt)
        return AVERROR(ENOMEM);

    rhw->hwaccel_ctx = vt;
    rhw->hwaccel_uninit = videotoolbox_uninit;
    rhw->hwaccel_retrieve_data = videotoolbox_retrieve_data;

    vt->tmp_frame = av_frame_alloc();
    if (!vt->tmp_frame) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    if (rhw->hwaccel_id == HWACCEL_VIDEOTOOLBOX) {
#if CONFIG_VIDEOTOOLBOX
        ret = av_videotoolbox_default_init(avctx);
#endif
    } else {
#if CONFIG_VDA
        ret = av_vda_default_init(avctx);
#endif
    }
    if (ret < 0) {
        const char *hwaccel_name = rhw->hwaccel_id == HWACCEL_VIDEOTOOLBOX ? "Videotoolbox" : "VDA";
        if (rhw->hwaccel_id == HWACCEL_AUTO)
            RING_DBG("Error creating %s decoder", hwaccel_name);
        else
            RING_ERR("Error creating %s decoder", hwaccel_name);
        goto fail;
    }

    return 0;
fail:
    videotoolbox_uninit(avctx);
    return ret;
}

#endif // USE_HWACCEL
