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
#include <stdint.h>

#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>

#include <X11/Xlib.h>

#include "libavcodec/vdpau.h"
#include "libavutil/buffer.h"
#include "libavutil/frame.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_vdpau.h"
}

#include "video/hwaccel.h"

#include "logger.h"

typedef struct VDPAUContext {
    AVBufferRef *hw_frames_ctx;
    AVFrame *tmp_frame;
} VDPAUContext;

typedef struct VDPAUHWDevicePriv {
    VdpDeviceDestroy *device_destroy;
    Display *dpy;
} VDPAUHWDevicePriv;

static void device_free(AVHWDeviceContext *ctx)
{
    AVVDPAUDeviceContext *hwctx = static_cast<AVVDPAUDeviceContext*>(ctx->hwctx);
    VDPAUHWDevicePriv *priv = static_cast<VDPAUHWDevicePriv*>(ctx->user_opaque);

    if (priv->device_destroy)
        priv->device_destroy(hwctx->device);
    if (priv->dpy)
        XCloseDisplay(priv->dpy);
    av_freep(&priv);
}

static void vdpau_uninit(AVCodecContext *avctx)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    VDPAUContext *ctx = static_cast<VDPAUContext*>(rhw->hwaccel_ctx);

    rhw->hwaccel_uninit = NULL;
    rhw->hwaccel_get_buffer = NULL;
    rhw->hwaccel_retrieve_data = NULL;

    av_buffer_unref(&ctx->hw_frames_ctx);
    av_frame_free(&ctx->tmp_frame);

    av_freep(&rhw->hwaccel_ctx);
    av_freep(&avctx->hwaccel_context);
}

static int vdpau_get_buffer(AVCodecContext *avctx, AVFrame *frame, int flags)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    VDPAUContext *ctx = static_cast<VDPAUContext*>(rhw->hwaccel_ctx);

    return av_hwframe_get_buffer(ctx->hw_frames_ctx, frame, 0);
}

static int vdpau_retrieve_data(AVCodecContext *avctx, AVFrame *frame)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    VDPAUContext *ctx = static_cast<VDPAUContext*>(rhw->hwaccel_ctx);
    int ret;

    ret = av_hwframe_transfer_data(ctx->tmp_frame, frame, 0);
    if (ret < 0)
        return ret;

    ret = av_frame_copy_props(ctx->tmp_frame, frame);
    if (ret < 0) {
        av_frame_unref(ctx->tmp_frame);
        return ret;
    }

    av_frame_unref(frame);
    av_frame_move_ref(frame, ctx->tmp_frame);

    return 0;
}

static int vdpau_alloc(AVCodecContext *avctx)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    VDPAUContext *ctx;
    const char *display, *vendor;
    VdpStatus err;
    int ret;

    VdpDevice device;
    VdpGetProcAddress *get_proc_address;
    VdpGetInformationString *get_information_string;

    VDPAUHWDevicePriv *device_priv = NULL;
    AVBufferRef   *device_ref = NULL;
    AVHWDeviceContext   *device_ctx;
    AVVDPAUDeviceContext *device_hwctx;
    AVHWFramesContext *frames_ctx;

    ctx = static_cast<VDPAUContext*>(av_mallocz(sizeof(*ctx)));
    if (!ctx)
        return AVERROR(ENOMEM);

    device_priv = static_cast<VDPAUHWDevicePriv*>(av_mallocz(sizeof(*device_priv)));
    if (!device_priv) {
        av_freep(&ctx);
        goto fail;
    }

    rhw->hwaccel_ctx = ctx;
    rhw->hwaccel_uninit = vdpau_uninit;
    rhw->hwaccel_get_buffer = vdpau_get_buffer;
    rhw->hwaccel_retrieve_data = vdpau_retrieve_data;

    ctx->tmp_frame = av_frame_alloc();
    if (!ctx->tmp_frame)
        goto fail;

    device_priv->dpy = XOpenDisplay(rhw->hwaccel_device);
    if (!device_priv->dpy) {
        if (rhw->auto_detect)
            RING_DBG("Cannot open the X11 display %s", XDisplayName(rhw->hwaccel_device));
        else
            RING_ERR("Cannot open the X11 display %s", XDisplayName(rhw->hwaccel_device));
        goto fail;
    }
    display = XDisplayString(device_priv->dpy);

    RING_WARN("About to create vdpau x11 device: %s", display);
    return -1;
    err = vdp_device_create_x11(device_priv->dpy, XDefaultScreen(device_priv->dpy),
                                &device, &get_proc_address);
    if (err != VDP_STATUS_OK) {
        if (rhw->auto_detect)
            RING_DBG("VDPAU device creation on X11 display %s failed", display);
        else
            RING_ERR("VDPAU device creation on X11 display %s failed", display);
        goto fail;
    }

#define GET_CALLBACK(id, result, ret_type)                                      \
do {                                                                            \
    void *tmp;                                                                  \
    err = get_proc_address(device, id, &tmp);                                   \
    if (err != VDP_STATUS_OK) {                                                 \
        if (rhw->auto_detect)                                                   \
            RING_DBG("Error getting the " #id " callback");                     \
        else                                                                    \
            RING_ERR("Error getting the " #id " callback");                     \
        goto fail;                                                              \
    }                                                                           \
    result = (ret_type)tmp;                                                     \
} while (0)

    GET_CALLBACK(VDP_FUNC_ID_GET_INFORMATION_STRING, get_information_string, VdpStatus (*)(const char**));
    GET_CALLBACK(VDP_FUNC_ID_DEVICE_DESTROY, device_priv->device_destroy, VdpStatus (*)(VdpDevice));

    device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VDPAU);
    if (!device_ref)
        goto fail;
    device_ctx                     = (AVHWDeviceContext*)device_ref->data;
    device_hwctx                   = static_cast<AVVDPAUDeviceContext*>(device_ctx->hwctx);
    device_ctx->user_opaque        = device_priv;
    device_ctx->free               = device_free;
    device_hwctx->device           = device;
    device_hwctx->get_proc_address = get_proc_address;

    device_priv = NULL;

    ret = av_hwdevice_ctx_init(device_ref);
    if (ret < 0)
        goto fail;

    ctx->hw_frames_ctx = av_hwframe_ctx_alloc(device_ref);
    if (!ctx->hw_frames_ctx)
        goto fail;
    av_buffer_unref(&device_ref);

    frames_ctx = (AVHWFramesContext*)ctx->hw_frames_ctx->data;
    frames_ctx->format = AV_PIX_FMT_VDPAU;
    frames_ctx->sw_format = avctx->sw_pix_fmt;
    frames_ctx->width = avctx->coded_width;
    frames_ctx->height = avctx->coded_height;

    ret = av_hwframe_ctx_init(ctx->hw_frames_ctx);
    if (ret < 0)
        goto fail;

    if (av_vdpau_bind_context(avctx, device, get_proc_address, 0))
        goto fail;

    get_information_string(&vendor);
    RING_DBG("Using VDPAU -- %s -- on X11 display %s, to decode input stream",
            vendor, display);

    return 0;

fail:
    if (rhw->auto_detect)
        RING_DBG("VDPAU init failed for stream");
    else
        RING_ERR("VDPAU init failed for stream");
    if (device_priv) {
        if (device_priv->device_destroy)
            device_priv->device_destroy(device);
        if (device_priv->dpy)
            XCloseDisplay(device_priv->dpy);
    }
    av_freep(&device_priv);
    av_buffer_unref(&device_ref);
    vdpau_uninit(avctx);
    return AVERROR(EINVAL);
}

int vdpau_init(AVCodecContext *avctx)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);

    if (!rhw->hwaccel_ctx) {
        int ret = vdpau_alloc(avctx);
        if (ret < 0)
            return ret;
    }

    rhw->hwaccel_get_buffer = vdpau_get_buffer;
    rhw->hwaccel_retrieve_data = vdpau_retrieve_data;

    return 0;
}

#endif
