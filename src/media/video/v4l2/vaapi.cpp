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

#if RING_VIDEO && USE_HWACCEL

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <va/va.h>
#if HAVE_VAAPI_DRM
#   include <va/va_drm.h>
#endif
#if HAVE_VAAPI_X11
#   include <va/va_x11.h>
#endif

#include <libavutil/avconfig.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

#include <libavcodec/vaapi.h>
}

#include "video/hwaccel.h"

#include "logger.h"

// these fields need to be in the same order as the definition in libavutil.h
// so the option field had to be added because we want the version field
static AVClass vaapi_class {
    .class_name = "vaapi",
    .item_name  = av_default_item_name,
    .option     = NULL,
    .version    = LIBAVUTIL_VERSION_INT,
};

#define DEFAULT_SURFACES 20

typedef struct VAAPIDecoderContext {
    const AVClass *av_class;

    AVBufferRef       *device_ref;
    AVHWDeviceContext *device;
    AVBufferRef       *frames_ref;
    AVHWFramesContext *frames;

    VAProfile    va_profile;
    VAEntrypoint va_entrypoint;
    VAConfigID   va_config;
    VAContextID  va_context;

    enum AVPixelFormat decode_format;
    int decode_width;
    int decode_height;
    int decode_surfaces;

    // The output need not have the same format, width and height as the
    // decoded frames - the copy for non-direct-mapped access is actually
    // a whole vpp instance which can do arbitrary scaling and format
    // conversion.
    enum AVPixelFormat output_format;

    struct vaapi_context decoder_vaapi_context;
} VAAPIDecoderContext;


static int vaapi_get_buffer(AVCodecContext *avctx, AVFrame *frame, int flags)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    VAAPIDecoderContext *ctx = static_cast<VAAPIDecoderContext*>(rhw->hwaccel_ctx);
    int err;

    if (!(avctx->codec->capabilities & CODEC_CAP_DR1))
        return avcodec_default_get_buffer2(avctx, frame, flags);

    err = av_hwframe_get_buffer(ctx->frames_ref, frame, 0);
    if (err < 0)
        RING_ERR("Failed to allocate decoder surface");
    // else
    //     RING_DBG("Decoder given surface %#x", (unsigned int)(uintptr_t)frame->data[3]);
    return err;
}

static int vaapi_retrieve_data(AVCodecContext *avctx, AVFrame *input)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    VAAPIDecoderContext *ctx = static_cast<VAAPIDecoderContext*>(rhw->hwaccel_ctx);
    AVFrame *output = 0;
    int err;

    if (input->format != AV_PIX_FMT_VAAPI) {
        RING_ERR("Frame format is not vaapi, it is %s",
                 av_get_pix_fmt_name((AVPixelFormat)input->format));
        abort(); // will segfault anyway, so just abort
    }

    if (ctx->output_format == AV_PIX_FMT_VAAPI) {
        // Nothing to do.
        return 0;
    }

    // RING_DBG("Retrieve data from surface %#x",
    //        (unsigned int)(uintptr_t)input->data[3]);

    output = av_frame_alloc();
    if (!output)
        return AVERROR(ENOMEM);

    output->format = ctx->output_format;

    err = av_hwframe_transfer_data(output, input, 0);
    if (err < 0) {
        RING_ERR("Failed to transfer data to output frame: %d", err);
        goto fail;
    }

    err = av_frame_copy_props(output, input);
    if (err < 0) {
        av_frame_unref(output);
        goto fail;
    }

    av_frame_unref(input);
    av_frame_move_ref(input, output);
    av_frame_free(&output);

    return 0;

fail:
    RING_ERR("Failed to retrieve vaapi data");
    if (output)
        av_frame_free(&output);
    return err;
}


static const struct {
    enum AVCodecID codec_id;
    int codec_profile;
    VAProfile va_profile;
} vaapi_profile_map[] = {
#define MAP(c, p, v) { AV_CODEC_ID_ ## c, FF_PROFILE_ ## p, VAProfile ## v }
    MAP(MPEG2VIDEO,  MPEG2_SIMPLE,    MPEG2Simple ),
    MAP(MPEG2VIDEO,  MPEG2_MAIN,      MPEG2Main   ),
    MAP(H263,        UNKNOWN,         H263Baseline),
    MAP(MPEG4,       MPEG4_SIMPLE,    MPEG4Simple ),
    MAP(MPEG4,       MPEG4_ADVANCED_SIMPLE,
                               MPEG4AdvancedSimple),
    MAP(MPEG4,       MPEG4_MAIN,      MPEG4Main   ),
    MAP(H264,        H264_CONSTRAINED_BASELINE,
                           H264ConstrainedBaseline),
    MAP(H264,        H264_BASELINE,   H264Baseline),
    MAP(H264,        H264_MAIN,       H264Main    ),
    MAP(H264,        H264_HIGH,       H264High    ),
#if VA_CHECK_VERSION(0, 37, 0)
    MAP(HEVC,        HEVC_MAIN,       HEVCMain    ),
#endif
    MAP(WMV3,        VC1_SIMPLE,      VC1Simple   ),
    MAP(WMV3,        VC1_MAIN,        VC1Main     ),
    MAP(WMV3,        VC1_COMPLEX,     VC1Advanced ),
    MAP(WMV3,        VC1_ADVANCED,    VC1Advanced ),
    MAP(VC1,         VC1_SIMPLE,      VC1Simple   ),
    MAP(VC1,         VC1_MAIN,        VC1Main     ),
    MAP(VC1,         VC1_COMPLEX,     VC1Advanced ),
    MAP(VC1,         VC1_ADVANCED,    VC1Advanced ),
#if VA_CHECK_VERSION(0, 35, 0)
    MAP(VP8,         UNKNOWN,       VP8Version0_3 ),
#endif
#if VA_CHECK_VERSION(0, 37, 1)
    MAP(VP9,         VP9_0,           VP9Profile0 ),
#endif
#undef MAP
};

static int vaapi_build_decoder_config(VAAPIDecoderContext *ctx,
                                      AVCodecContext *avctx,
                                      int fallback_allowed)
{
    AVVAAPIDeviceContext *hwctx = static_cast<AVVAAPIDeviceContext*>(ctx->device->hwctx);
    AVVAAPIHWConfig *hwconfig = NULL;
    AVHWFramesConstraints *constraints = NULL;
    VAStatus vas;
    int err, i, j;
    const AVCodecDescriptor *codec_desc;
    const AVPixFmtDescriptor *pix_desc;
    enum AVPixelFormat pix_fmt;
    VAProfile profile, *profile_list = NULL;
    int profile_count, exact_match, alt_profile;

    codec_desc = avcodec_descriptor_get(avctx->codec_id);
    if (!codec_desc) {
        err = AVERROR(EINVAL);
        goto fail;
    }

    profile_count = vaMaxNumProfiles(hwctx->display);
    profile_list = static_cast<VAProfile*>(av_malloc(profile_count * sizeof(VAProfile)));
    if (!profile_list) {
        err = AVERROR(ENOMEM);
        goto fail;
    }

    vas = vaQueryConfigProfiles(hwctx->display,
                                profile_list, &profile_count);
    if (vas != VA_STATUS_SUCCESS) {
        if (fallback_allowed)
            RING_DBG("Failed to query profiles: %d (%s)", vas, vaErrorStr(vas));
        else
            RING_ERR("Failed to query profiles: %d (%s)", vas, vaErrorStr(vas));
        err = AVERROR(EIO);
        goto fail;
    }

    profile = VAProfileNone;
    exact_match = 0;

    for (i = 0; i < (sizeof(vaapi_profile_map)/sizeof(vaapi_profile_map[0])); i++) {
        int profile_match = 0;
        if (avctx->codec_id != vaapi_profile_map[i].codec_id)
            continue;
        if (avctx->profile == vaapi_profile_map[i].codec_profile)
            profile_match = 1;
        profile = vaapi_profile_map[i].va_profile;
        for (j = 0; j < profile_count; j++) {
            if (profile == profile_list[j]) {
                exact_match = profile_match;
                break;
            }
        }
        if (j < profile_count) {
            if (exact_match)
                break;
            alt_profile = vaapi_profile_map[i].codec_profile;
        }
    }
    av_freep(&profile_list);

    if (profile == VAProfileNone) {
        if (fallback_allowed)
            RING_DBG("No VAAPI support for codec %s", codec_desc->name);
        else
            RING_ERR("No VAAPI support for codec %s", codec_desc->name);
        err = AVERROR(ENOSYS);
        goto fail;
    }
    if (!exact_match) {
        if (fallback_allowed) {
            RING_DBG("No VAAPI support for codec %s profile %d", codec_desc->name, avctx->profile);
            err = AVERROR(EINVAL);
            goto fail;
        } else {
            RING_WARN("No VAAPI support for codec %s profile %d: trying instead with profile %d",
                   codec_desc->name, avctx->profile, alt_profile);
            RING_WARN("This may fail or give incorrect results, depending on your hardware");
        }
    }

    ctx->va_profile = profile;
    ctx->va_entrypoint = VAEntrypointVLD;

    vas = vaCreateConfig(hwctx->display, ctx->va_profile,
                         ctx->va_entrypoint, 0, 0, &ctx->va_config);
    if (vas != VA_STATUS_SUCCESS) {
        av_log(ctx, AV_LOG_ERROR, "Failed to create decode pipeline "
               "configuration: %d (%s).\n", vas, vaErrorStr(vas));
        err = AVERROR(EIO);
        goto fail;
    }

    hwconfig = static_cast<AVVAAPIHWConfig*>(av_hwdevice_hwconfig_alloc(ctx->device_ref));
    if (!hwconfig) {
        err = AVERROR(ENOMEM);
        goto fail;
    }
    hwconfig->config_id = ctx->va_config;

    constraints = av_hwdevice_get_hwframe_constraints(ctx->device_ref,
                                                      hwconfig);
    if (!constraints)
        goto fail;

    ctx->decode_format = AV_PIX_FMT_NONE;

    // Assume for now that we are always dealing with YUV 4:2:0, so
    // pick a format which does that
    for (i = 0; constraints->valid_sw_formats[i] != AV_PIX_FMT_NONE; i++) {
        pix_fmt  = constraints->valid_sw_formats[i];
        pix_desc = av_pix_fmt_desc_get(pix_fmt);
        if (pix_desc->nb_components == 3 &&
            pix_desc->log2_chroma_w == 1 &&
            pix_desc->log2_chroma_h == 1) {
            ctx->decode_format = pix_fmt;
            RING_DBG("Using decode format %s (format matched)",
                     av_get_pix_fmt_name(ctx->decode_format));
            break;
        }
    }

    // Otherwise pick the first in the list and hope for the best.
    if (ctx->decode_format == AV_PIX_FMT_NONE) {
        ctx->decode_format = constraints->valid_sw_formats[0];
        RING_DBG("Using decode format %s (first in list)",
               av_get_pix_fmt_name(ctx->decode_format));
        if (i > 1) {
            // There was a choice, and we picked randomly.  Warn the user
            // that they might want to choose intelligently instead.
            RING_WARN("Using randomly chosen decode format %s",
                      av_get_pix_fmt_name(ctx->decode_format));
        }
    }

    // Ensure the picture size is supported by the hardware.
    ctx->decode_width  = avctx->coded_width;
    ctx->decode_height = avctx->coded_height;
    if (ctx->decode_width  < constraints->min_width  ||
        ctx->decode_height < constraints->min_height ||
        ctx->decode_width  > constraints->max_width  ||
        ctx->decode_height >constraints->max_height) {
        RING_ERR("VAAPI hardware does not support image size %dx%d (constraints: width %d-%d height %d-%d)",
               ctx->decode_width, ctx->decode_height,
               constraints->min_width,  constraints->max_width,
               constraints->min_height, constraints->max_height);
        err = AVERROR(EINVAL);
        goto fail;
    }

    av_hwframe_constraints_free(&constraints);
    av_freep(&hwconfig);

    // Decide how many reference frames we need.  This might be doable more
    // nicely based on the codec and input stream?
    ctx->decode_surfaces = DEFAULT_SURFACES;
    // For frame-threaded decoding, one additional surfaces is needed for
    // each thread.
    if (avctx->active_thread_type & FF_THREAD_FRAME)
        ctx->decode_surfaces += avctx->thread_count;

    return 0;

fail:
    av_hwframe_constraints_free(&constraints);
    av_freep(&hwconfig);
    vaDestroyConfig(hwctx->display, ctx->va_config);
    av_free(profile_list);
    return err;
}

static void vaapi_decode_uninit(AVCodecContext *avctx)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    VAAPIDecoderContext *ctx = static_cast<VAAPIDecoderContext*>(rhw->hwaccel_ctx);

    if (ctx) {
        AVVAAPIDeviceContext *hwctx = static_cast<AVVAAPIDeviceContext*>(ctx->device->hwctx);

        if (ctx->va_context != VA_INVALID_ID) {
            vaDestroyContext(hwctx->display, ctx->va_context);
            ctx->va_context = VA_INVALID_ID;
        }
        if (ctx->va_config != VA_INVALID_ID) {
            vaDestroyConfig(hwctx->display, ctx->va_config);
            ctx->va_config = VA_INVALID_ID;
        }

        av_buffer_unref(&ctx->frames_ref);
        av_buffer_unref(&ctx->device_ref);
        av_free(ctx);
    }

    av_buffer_unref(&rhw->hw_frames_ctx);

    rhw->hwaccel_ctx = 0;
    rhw->hwaccel_uninit = 0;
    rhw->hwaccel_get_buffer = 0;
    rhw->hwaccel_retrieve_data = 0;
}

static void vaapi_device_uninit(AVHWDeviceContext *hwdev)
{
    AVVAAPIDeviceContext *hwctx = static_cast<AVVAAPIDeviceContext*>(hwdev->hwctx);
    RING_DBG("Terminating VAAPI connection");
    vaTerminate(hwctx->display);
}

static AVBufferRef *vaapi_device_init(const char *device, int *err)
{
    AVHWDeviceContext    *hwdev;
    AVVAAPIDeviceContext *hwctx;
    VADisplay display;
    VAStatus vas;
    AVBufferRef *hw_device_ctx;
    int major, minor;

    display = 0;

#ifdef HAVE_VAAPI_DRM
    if (!display && device) {
        int drm_fd;

        // Try to open the device as a DRM path.
        drm_fd = open(device, O_RDWR);
        if (drm_fd < 0) {
            RING_WARN("Cannot open DRM device %s", device);
        } else {
            display = vaGetDisplayDRM(drm_fd);
            if (!display) {
                RING_WARN("Cannot open a VA display from DRM device %s", device);
                close(drm_fd);
            } else {
                RING_DBG("Opened VA display via DRM device %s", device);
            }
        }
    }
#endif

#ifdef HAVE_VAAPI_X11
    if (!display) {
        Display *x11_display;

        // Try to open the device as an X11 display.
        x11_display = XOpenDisplay(device);
        if (!x11_display) {
            RING_WARN("Cannot open X11 display %s", XDisplayName(device));
        } else {
            display = vaGetDisplay(x11_display);
            if (!display) {
                RING_WARN("Cannot open a VA display from X11 display %s", XDisplayName(device));
                XCloseDisplay(x11_display);
            } else {
                RING_DBG("Opened VA display via X11 display %s", XDisplayName(device));
            }
        }
    }
#endif

    if (!display) {
        RING_ERR("No VA display found for device %s", device);
        *err = AVERROR(EINVAL);
        return NULL;
    }

    vas = vaInitialize(display, &major, &minor);
    if (vas != VA_STATUS_SUCCESS) {
        RING_ERR("Failed to initialise VAAPI connection: %d (%s)", vas, vaErrorStr(vas));
        *err = AVERROR(EIO);
        return NULL;
    }
    RING_DBG("Initialised VAAPI connection: version %d.%d", major, minor);

    hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!hw_device_ctx) {
        RING_ERR("Failed to create VAAPI hardware context");
        vaTerminate(display);
        *err = AVERROR(ENOMEM);
        return NULL;
    }

    hwdev = (AVHWDeviceContext*)hw_device_ctx->data;
    hwdev->free = &vaapi_device_uninit;

    hwctx = static_cast<AVVAAPIDeviceContext*>(hwdev->hwctx);
    hwctx->display = display;

    *err = av_hwdevice_ctx_init(hw_device_ctx);
    if (*err < 0) {
        RING_ERR("Failed to initialise VAAPI hardware context: %d", *err);
        return NULL;
    }

    return hw_device_ctx;
}

int vaapi_decode_init(AVCodecContext *avctx)
{
    RingHWContext *rhw = static_cast<RingHWContext*>(avctx->opaque);
    AVVAAPIDeviceContext *hwctx;
    AVVAAPIFramesContext *avfc;
    VAAPIDecoderContext *ctx;
    VAStatus vas;
    int err;

    if (rhw->hwaccel_ctx)
        vaapi_decode_uninit(avctx);

    AVBufferRef *hw_device_ctx;
    hw_device_ctx = vaapi_device_init(rhw->hwaccel_device, &err);
    if (err < 0)
        return err;

    ctx = static_cast<VAAPIDecoderContext*>(av_mallocz(sizeof(*ctx)));
    if (!ctx)
        return AVERROR(ENOMEM);
    ctx->av_class = &vaapi_class;

    ctx->device_ref = av_buffer_ref(hw_device_ctx);
    ctx->device = (AVHWDeviceContext*)ctx->device_ref->data;

    ctx->va_config  = VA_INVALID_ID;
    ctx->va_context = VA_INVALID_ID;

    hwctx = static_cast<AVVAAPIDeviceContext*>(ctx->device->hwctx);

    // this can be set to none for now (default)
    ctx->output_format = AV_PIX_FMT_NONE;

    err = vaapi_build_decoder_config(ctx, avctx, rhw->hwaccel_id != HWACCEL_VAAPI);
    if (err < 0) {
        RING_WARN("No supported configuration for this codec.");
        goto fail;
    }

    avctx->pix_fmt = ctx->output_format;

    ctx->frames_ref = av_hwframe_ctx_alloc(ctx->device_ref);
    if (!ctx->frames_ref) {
        RING_ERR("Failed to create VAAPI frame context");
        err = AVERROR(ENOMEM);
        goto fail;
    }

    ctx->frames = (AVHWFramesContext*)ctx->frames_ref->data;

    ctx->frames->format = AV_PIX_FMT_VAAPI;
    ctx->frames->sw_format = ctx->decode_format;
    ctx->frames->width = ctx->decode_width;
    ctx->frames->height = ctx->decode_height;
    ctx->frames->initial_pool_size = ctx->decode_surfaces;

    err = av_hwframe_ctx_init(ctx->frames_ref);
    if (err < 0) {
        RING_ERR("Failed to initialise VAAPI frame context: %d", err);
        goto fail;
    }

    avfc = static_cast<AVVAAPIFramesContext*>(ctx->frames->hwctx);

    vas = vaCreateContext(hwctx->display, ctx->va_config,
                          ctx->decode_width, ctx->decode_height,
                          VA_PROGRESSIVE,
                          avfc->surface_ids, avfc->nb_surfaces,
                          &ctx->va_context);
    if (vas != VA_STATUS_SUCCESS) {
        RING_ERR("Failed to create decode pipeline context: %d (%s)", vas, vaErrorStr(vas));
        err = AVERROR(EINVAL);
        goto fail;
    }

    RING_DBG("VAAPI decoder (re)init complete");

    // We would like to set this on the AVCodecContext for use by whoever gets
    // the frames from the decoder, but unfortunately the AVCodecContext we
    // have here need not be the "real" one (H.264 makes many copies for
    // threading purposes).  To avoid the problem, we instead store it in the
    // InputStream and propagate it from there.
    rhw->hw_frames_ctx = av_buffer_ref(ctx->frames_ref);
    if (!rhw->hw_frames_ctx) {
        err = AVERROR(ENOMEM);
        goto fail;
    }

    rhw->hwaccel_ctx = ctx;
    rhw->hwaccel_uninit = vaapi_decode_uninit;
    rhw->hwaccel_get_buffer = vaapi_get_buffer;
    rhw->hwaccel_retrieve_data = vaapi_retrieve_data;

    ctx->decoder_vaapi_context.display    = hwctx->display;
    ctx->decoder_vaapi_context.config_id  = ctx->va_config;
    ctx->decoder_vaapi_context.context_id = ctx->va_context;
    avctx->hwaccel_context = &ctx->decoder_vaapi_context;

    avctx->draw_horiz_band = 0;
    avctx->slice_flags = SLICE_FLAG_CODED_ORDER|SLICE_FLAG_ALLOW_FIELD;

    return 0;

fail:
    vaapi_decode_uninit(avctx);
    return err;
}

#endif // USE_HWACCEL
