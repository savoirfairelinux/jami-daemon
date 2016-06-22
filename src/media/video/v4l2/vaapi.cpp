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

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "config.h"

#if defined(RING_VIDEO) && defined(RING_ACCEL)

#include "video/v4l2/vaapi.h"
#include "video/accel.h"

#include <stdexcept>

#include "logger.h"

namespace ring { namespace video {

void vaapi_device_uninit(AVHWDeviceContext *hwdev)
{
    AVVAAPIDeviceContext *hwctx = static_cast<AVVAAPIDeviceContext*>(hwdev->hwctx);
    RING_DBG("Terminating VAAPI connection");
    vaTerminate(hwctx->display);
}

VaapiAccel::VaapiAccel()
{
}

VaapiAccel::~VaapiAccel()
{
}

int
VaapiAccel::allocateBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags)
{
    int err = av_hwframe_get_buffer(frames_ref, frame, 0);
    if (err < 0)
        RING_ERR("Failed to allocate decoder surface");
    else
        RING_DBG("Decoder given surface %#x", (unsigned int)(uintptr_t)frame->data[3]);
    return err;
}

bool
VaapiAccel::extractData(AVCodecContext* codecCtx, VideoFrame& container)
{
    auto input = container.pointer();

    if (input->format != format_) {
        throw std::runtime_error("Frame format is not VAAPI");
    }

    RING_DBG("Extracting from surface %#x", (unsigned int)(uintptr_t)input->data[3]);

    auto outContainer = new VideoFrame();
    auto output = outContainer->pointer();
    output->format = AV_PIX_FMT_YUV420P;

    if (av_hwframe_transfer_data(output, input, 0) < 0) {
        throw std::runtime_error("Unable to extract data from VAAPI frame");
    }

    if (av_frame_copy_props(output, input) < 0 ) {
        av_frame_unref(output);
        throw std::runtime_error("Copy props failed");
    }

    av_frame_unref(input);
    av_frame_move_ref(input, output);

    return true;
}

void
VaapiAccel::init(AVCodecContext* codecCtx)
{
    AVVAAPIDeviceContext* hwctx;
    AVVAAPIFramesContext* avfc;
    VAStatus vas;

#ifdef HAVE_VAAPI_DRM
    const char* deviceName = "/dev/dri/card0"; // check for renderDX first?
#else
    const char* deviceName = ":0";
#endif

    if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, deviceName, NULL, 0) < 0) {
        throw std::runtime_error("Failed to create VAAPI device");
    }

    device_ref = av_buffer_ref(hw_device_ctx);
    device = (AVHWDeviceContext*)device_ref->data;
    va_config = VA_INVALID_ID;
    va_context = VA_INVALID_ID;
    hwctx = (AVVAAPIDeviceContext*)device->hwctx;

    AVVAAPIHWConfig* hwconfig;
    AVHWFramesConstraints* constraints;
    const AVCodecDescriptor* codec_desc;
    const AVPixFmtDescriptor* pix_desc;
    enum AVPixelFormat pix_fmt;
    VAProfile profile;
    VAProfile* profile_list;
    int profile_count;

    codec_desc = avcodec_descriptor_get(codecCtx->codec_id);
    profile_count = vaMaxNumProfiles(hwctx->display);
    profile_list = static_cast<VAProfile*>(av_mallocz(profile_count * sizeof(VAProfile)));

    vas = vaQueryConfigProfiles(hwctx->display, profile_list, &profile_count);
    if (vas != VA_STATUS_SUCCESS) {
        throw std::runtime_error("Failed to query profiles");
    }

    profile = VAProfileNone;
    switch(codecCtx->codec_id) {
        case AV_CODEC_ID_H264:
            profile = VAProfileH264High;
            break;
        case AV_CODEC_ID_MPEG4:
            profile = VAProfileMPEG4AdvancedSimple;
            break;
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
            profile = VAProfileH263Baseline;
            break;
        default:
            throw std::runtime_error("Codec not supported for VAAPI");
    }
    av_freep(&profile_list);
    va_profile = profile;
    va_entrypoint = VAEntrypointVLD;

    vas = vaCreateConfig(hwctx->display, va_profile, va_entrypoint, 0, 0, &va_config);
    if (vas != VA_STATUS_SUCCESS) {
        throw std::runtime_error("Failed to create decode pipeline configuration");
    }

    hwconfig = (AVVAAPIHWConfig*)av_hwdevice_hwconfig_alloc(device_ref);
    hwconfig->config_id = va_config;

    constraints = av_hwdevice_get_hwframe_constraints(device_ref, hwconfig);

    decode_format = AV_PIX_FMT_NONE;
    for (int i = 0; constraints->valid_sw_formats[i] != AV_PIX_FMT_NONE; i++) {
        pix_fmt = constraints->valid_sw_formats[i];
        pix_desc = av_pix_fmt_desc_get(pix_fmt);
        if (pix_desc->nb_components == 3
            && pix_desc->log2_chroma_w == 1
            && pix_desc->log2_chroma_h == 1) {
            decode_format = pix_fmt;
        }
    }

    if (width_ < constraints->min_width
        || width_ > constraints->max_width
        || height_ < constraints->min_height
        || height_ > constraints->max_height) {
        throw std::runtime_error("VAAPI hardware does not support image size");
    }

    av_hwframe_constraints_free(&constraints);
    av_freep(&hwconfig);

    decode_surfaces = 16; // based on codec instead?
    if (codecCtx->active_thread_type & FF_THREAD_FRAME)
        decode_surfaces += codecCtx->thread_count; // extra surface per thread

    frames_ref = av_hwframe_ctx_alloc(device_ref);
    frames = (AVHWFramesContext*)frames_ref->data;
    frames->format = AV_PIX_FMT_VAAPI;
    frames->sw_format = decode_format;
    frames->width = width_;
    frames->height = height_;
    frames->initial_pool_size = decode_surfaces;

    if (av_hwframe_ctx_init(frames_ref) < 0) {
        throw std::runtime_error("Failed to initialize VAAPI frame context");
    }

    avfc = (AVVAAPIFramesContext*)frames->hwctx;
    vas = vaCreateContext(hwctx->display, va_config, width_, height_, VA_PROGRESSIVE, avfc->surface_ids, avfc->nb_surfaces, &va_context);
    if (vas != VA_STATUS_SUCCESS) {
        throw std::runtime_error("Failed to create decode pipeline context");
    }

    RING_DBG("VAAPI decoder init complete");

    hw_frames_ctx = av_buffer_ref(frames_ref);

    decoder_vaapi_context.display = hwctx->display;
    decoder_vaapi_context.config_id = va_config;
    decoder_vaapi_context.context_id = va_context;
    codecCtx->hwaccel_context = (void*)&decoder_vaapi_context;
}

}}

#endif // defined(RING_VIDEO) && defined(RING_ACCEL)
