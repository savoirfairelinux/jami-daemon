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

#include "logger.h"

#include "../hwaccel.h"
#include "vaapi.h"

namespace ring { namespace video {

#define DEFAULT_SURFACES 20

VaapiAccel::VaapiAccel()
{
    name_ = RING_VAAPI;
    pixFmt_ = AV_PIX_FMT_VAAPI;
}

VaapiAccel::~VaapiAccel()
{
    auto hardwareCtx = static_cast<AVVAAPIDeviceContext*>(device_->hwctx);

    RING_DBG("Terminating VAAPI connection");

    if (vaContext_ != VA_INVALID_ID) {
        vaDestroyContext(hardwareCtx->display, vaContext_);
        vaContext_ = VA_INVALID_ID;
    }

    if (vaConfig_ != VA_INVALID_ID) {
        vaDestroyConfig(hardwareCtx->display, vaConfig_);
        vaConfig_ = VA_INVALID_ID;
    }

    av_buffer_unref(&framesRef_);
    av_buffer_unref(&deviceRef_);
}

static void deviceUninit(AVHWDeviceContext *hardwareDev)
{
    AVVAAPIDeviceContext* vaapiDevCtx = static_cast<AVVAAPIDeviceContext*>(hardwareDev->hwctx);
    RING_DBG("Terminating VAAPI connection");
    vaTerminate(vaapiDevCtx->display);
}

int
VaapiAccel::getBuffer(AVFrame* frame, int flags)
{
    int ret;

    if (!(codecCtx_->codec->capabilities & CODEC_CAP_DR1))
        return avcodec_default_get_buffer2(codecCtx_, frame, flags);

    ret = av_hwframe_get_buffer(framesRef_, frame, 0);
    if (ret < 0)
        RING_ERR("Failed to allocate decoder surface");

    return ret;
}

int
VaapiAccel::retrieveData(AVFrame* frame)
{
    AVFrame* output = 0;
    int ret;
    try {
        if (frame->format != AV_PIX_FMT_VAAPI) {
            RING_ERR("Frame format is not vaapi, it is %s",
                     av_get_pix_fmt_name((AVPixelFormat)frame->format));
            throw std::runtime_error("Incompatible frame format");
        }
        if (outputFmt_ == AV_PIX_FMT_VAAPI)
            return 0;

        RING_DBG("Retrieve data from surface %#x",
                 (unsigned int)(uintptr_t)frame->data[3]);

        output = av_frame_alloc();
        if (!output)
            throw AVERROR(ENOMEM);

        output->format = outputFmt_;

        ret = av_hwframe_transfer_data(output, frame, 0);
        if (ret < 0) {
            RING_ERR("Failed to transfer data to output frame");
            throw ret;
        }

        ret = av_frame_copy_props(output, frame);
        if (ret < 0) {
            av_frame_unref(output);
            throw ret;
        }

        av_frame_unref(frame);
        av_frame_move_ref(frame, output);
        av_frame_free(&output);
    } catch (int e) {
        RING_ERR("Failed to retrieve vaapi data: %d", e);
        if (output)
            av_frame_free(&output);
        return e;
    }
}

void
VaapiAccel::init(AVCodecContext* codecCtx)
{
    codecCtx_ = codecCtx;
    auto accelCtx = static_cast<HardwareAccelContext*>(codecCtx->opaque);
    AVVAAPIDeviceContext* hardwareCtx;
    AVVAAPIFramesContext* avFramesCtx;
    VAStatus status;
    int ret;

    AVBufferRef* hardwareDevCtx = deviceInit("", &ret);
    deviceRef_ = av_buffer_ref(hardwareDevCtx);
    device_ = (AVHWDeviceContext*)deviceRef_->data;

    vaConfig_ = VA_INVALID_ID;
    vaContext_ = VA_INVALID_ID;

    hardwareCtx = static_cast<AVVAAPIDeviceContext*>(device_->hwctx);

    outputFmt_ = AV_PIX_FMT_NONE;
    buildDecoderConfig(accelCtx->autoDetect());
    codecCtx->pix_fmt = outputFmt_;

    if (!(framesRef_ = av_hwframe_ctx_alloc(deviceRef_))) {
        RING_ERR("Failed to create VAAPI frame context");
        throw AVERROR(ENOMEM);
    }

    frames_ = (AVHWFramesContext*)framesRef_->data;
    frames_->format = AV_PIX_FMT_VAAPI;
    frames_->sw_format = decodeFmt_;
    frames_->width = decodeWidth_;
    frames_->height = decodeHeight_;
    frames_->initial_pool_size = decodeSurfaces_;

    if ((ret = av_hwframe_ctx_init(framesRef_)) < 0)  {
        RING_ERR("Failed to initialise VAAPI frame context: %d", ret);
        throw ret;
    }

    avFramesCtx = static_cast<AVVAAPIFramesContext*>(frames_->hwctx);

    status = vaCreateContext(hardwareCtx->display, vaConfig_,
                             decodeWidth_, decodeHeight_,
                             VA_PROGRESSIVE,
                             avFramesCtx->surface_ids, avFramesCtx->nb_surfaces,
                             &vaContext_);
    if (status != VA_STATUS_SUCCESS) {
        RING_ERR("Failed to create decode pipeline context: %d (%s)",
                 status, vaErrorStr(status));
        throw AVERROR(EINVAL);
    }

    RING_DBG("VAAPI decoder (re)init complete");

    decoderVaapiContext_.display = hardwareCtx->display;
    decoderVaapiContext_.config_id = vaConfig_;
    decoderVaapiContext_.context_id - vaContext_;
    codecCtx->hwaccel_context = &decoderVaapiContext_;

    codecCtx->draw_horiz_band = 0;
    codecCtx->slice_flags = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
}

void
VaapiAccel::buildDecoderConfig(int fallbackAllowed)
{
    auto hardwareCtx = static_cast<AVVAAPIDeviceContext*>(device_->hwctx);
    AVVAAPIHWConfig* hardwareConfig = nullptr;
    AVHWFramesConstraints* constraints = nullptr;
    VAStatus status;
    int ret, i, j;
    const AVCodecDescriptor* codecDesc = nullptr;
    const AVPixFmtDescriptor* pixDesc = nullptr;
    enum AVPixelFormat pixFmt;
    VAProfile profile;
    VAProfile* profileList = nullptr;
    int profileCount, exactMatch, altProfile;

    try {
        codecDesc = avcodec_descriptor_get(codecCtx_->codec_id);
        if (!codecDesc)
            throw AVERROR(EINVAL);

        profileCount = vaMaxNumProfiles(hardwareCtx->display);
        profileList = static_cast<VAProfile*>(av_malloc(profileCount * sizeof(VAProfile)));
        if (!profileList)
            throw AVERROR(ENOMEM);

        status = vaQueryConfigProfiles(hardwareCtx->display,
                                       profileList, &profileCount);
        if (status != VA_STATUS_SUCCESS) {
            if (fallbackAllowed)
                RING_DBG("Failed to query profiles: %d (%s)", status, vaErrorStr(status));
            else
                RING_ERR("Failed to query profiles: %d (%s)", status, vaErrorStr(status));
            throw AVERROR(EIO);
        }

        profile = VAProfileNone;
        exactMatch = 0;

        for (i = 0; i < (sizeof(vaapiProfileMap)/sizeof(vaapiProfileMap[0])); i++) {
            int profileMatch = 0;
            if (codecCtx_->codec_id != vaapiProfileMap[i].codecId)
                continue;
            if (codecCtx_->profile == vaapiProfileMap[i].codecProfile)
                profileMatch = 1;
            profile = vaapiProfileMap[i].vaProfile;
            for (j = 0; j < profileCount; j++) {
                if (profile == profileList[j]) {
                    exactMatch = profileMatch;
                    break;
                }
            }
            if (j < profileCount) {
                if (exactMatch)
                    break;
                altProfile = vaapiProfileMap[i].codecProfile;
            }
        }
        av_freep(&profileList);

        if (profile == VAProfileNone) {
            if (fallbackAllowed)
                RING_DBG("No VAAPI support for codec %s", codecDesc->name);
            else
                RING_ERR("No VAAPI support for codec %s", codecDesc->name);
            throw AVERROR(ENOSYS);
        }

        if (!exactMatch) {
            if (fallbackAllowed) {
                RING_DBG("No VAAPI support for codec %s profile %d", codecDesc->name, codecCtx_->profile);
                throw AVERROR(EINVAL);
            } else {
                RING_WARN("No VAAPI support for codec %s profile %d: trying instead with profile %d",
                       codecDesc->name, codecCtx_->profile, altProfile);
                RING_WARN("This may fail or give incorrect results, depending on your hardware");
            }
        }

        vaProfile_ = profile;
        vaEntrypoint_ = VAEntrypointVLD;

        status = vaCreateConfig(hardwareCtx->display, vaProfile_,
                             vaEntrypoint_, 0, 0, &vaConfig_);
        if (status != VA_STATUS_SUCCESS) {
            RING_ERR("Failed to create decode pipeline configuration: %d (%s)",
                     status, vaErrorStr(status));
            throw AVERROR(EIO);
        }

        hardwareConfig = static_cast<AVVAAPIHWConfig*>(av_hwdevice_hwconfig_alloc(deviceRef_));
        if (!hardwareConfig) {
            throw AVERROR(ENOMEM);
        }
        hardwareConfig->config_id = vaConfig_;

        constraints = av_hwdevice_get_hwframe_constraints(deviceRef_,
                                                          hardwareConfig);
        if (!constraints)
            throw std::runtime_error("av_hwdevice_get_hwframe_constraints returned nullptr");

        decodeFmt_ = AV_PIX_FMT_NONE;

        // Assume for now that we are always dealing with YUV 4:2:0, so
        // pick a format which does that
        for (i = 0; constraints->valid_sw_formats[i] != AV_PIX_FMT_NONE; i++) {
            pixFmt  = constraints->valid_sw_formats[i];
            pixDesc = av_pix_fmt_desc_get(pixFmt);
            if (pixDesc->nb_components == 3 &&
                pixDesc->log2_chroma_w == 1 &&
                pixDesc->log2_chroma_h == 1) {
                decodeFmt_ = pixFmt;
                RING_DBG("Using decode format %s (format matched)",
                         av_get_pix_fmt_name(decodeFmt_));
                break;
            }
        }

        // Otherwise pick the first in the list and hope for the best.
        if (decodeFmt_ == AV_PIX_FMT_NONE) {
            decodeFmt_ = constraints->valid_sw_formats[0];
            RING_DBG("Using decode format %s (first in list)",
                   av_get_pix_fmt_name(decodeFmt_));
            if (i > 1) {
                // There was a choice, and we picked randomly.  Warn the user
                // that they might want to choose intelligently instead.
                RING_WARN("Using randomly chosen decode format %s",
                          av_get_pix_fmt_name(decodeFmt_));
            }
        }

        // Ensure the picture size is supported by the hardware.
        decodeWidth_  = codecCtx_->coded_width;
        decodeHeight_ = codecCtx_->coded_height;
        if (decodeWidth_  < constraints->min_width  ||
            decodeHeight_ < constraints->min_height ||
            decodeWidth_  > constraints->max_width  ||
            decodeHeight_ >constraints->max_height) {
            RING_ERR("VAAPI hardware does not support image size %dx%d (constraints: width %d-%d height %d-%d)",
                   decodeWidth_, decodeHeight_,
                   constraints->min_width,  constraints->max_width,
                   constraints->min_height, constraints->max_height);
            throw AVERROR(EINVAL);
        }

        av_hwframe_constraints_free(&constraints);
        av_freep(&hardwareConfig);

        decodeSurfaces_ = DEFAULT_SURFACES;
        // For frame-threaded decoding, one additional surfaces is needed for
        // each thread.
        if (codecCtx_->active_thread_type & FF_THREAD_FRAME)
            decodeSurfaces_ += codecCtx_->thread_count;
    } catch (int e) {
        av_hwframe_constraints_free(&constraints);
        av_freep(&hardwareConfig);
        vaDestroyConfig(hardwareCtx->display, vaConfig_);
        av_free(profileList);
        throw std::runtime_error("Build VAAPI decoder config threw AVERROR: " + std::to_string(e));
    }
}

AVBufferRef*
VaapiAccel::deviceInit(const char *device, int *err)
{
    AVHWDeviceContext* hardwareDev;
    AVVAAPIDeviceContext* vaapiDevCtx;
    VADisplay display;
    VAStatus status;
    AVBufferRef* hardwareDevCtx;
    int major, minor;

    display = 0;

#ifdef HAVE_VAAPI_DRM
    if (!display && device) {
        int drmFd;

        // Try to open the device as a DRM path.
        drmFd = open(device, O_RDWR);
        if (drmFd < 0) {
            RING_WARN("Cannot open DRM device %s", device);
        } else {
            display = vaGetDisplayDRM(drmFd);
            if (!display) {
                RING_WARN("Cannot open a VA display from DRM device %s", device);
                close(drmFd);
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

    status = vaInitialize(display, &major, &minor);
    if (status != VA_STATUS_SUCCESS) {
        RING_ERR("Failed to initialise VAAPI connection: %d (%s)",
                 status, vaErrorStr(status));
        *err = AVERROR(EIO);
        return NULL;
    }
    RING_DBG("Initialised VAAPI connection: version %d.%d", major, minor);

    hardwareDevCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!hardwareDevCtx) {
        RING_ERR("Failed to create VAAPI hardware context");
        vaTerminate(display);
        *err = AVERROR(ENOMEM);
        return NULL;
    }

    hardwareDev = (AVHWDeviceContext*)hardwareDevCtx->data;
    hardwareDev->free = &deviceUninit;

    vaapiDevCtx = static_cast<AVVAAPIDeviceContext*>(hardwareDev->hwctx);
    vaapiDevCtx->display = display;

    *err = av_hwdevice_ctx_init(hardwareDevCtx);
    if (*err < 0) {
        RING_ERR("Failed to initialise VAAPI hardware context: %d", *err);
        return NULL;
    }

    return hardwareDevCtx;
}

}} // namespace ring::video
