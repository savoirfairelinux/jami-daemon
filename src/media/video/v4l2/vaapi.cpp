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

#include <sstream>
#include <stdexcept>
#include <map>

#include "logger.h"

namespace ring { namespace video {

VaapiAccel::VaapiAccel()
{
}

VaapiAccel::~VaapiAccel()
{
}

int
VaapiAccel::allocateBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags)
{
    return av_hwframe_get_buffer(framesBufferRef_, frame, 0);
}

bool
VaapiAccel::extractData(AVCodecContext* codecCtx, VideoFrame& container)
{
    auto input = container.pointer();

    if (input->format != format_) {
        fail();
        std::stringstream buf;
        buf << "Frame format mismatch: expected " << av_get_pix_fmt_name(format_);
        buf << ", got " << av_get_pix_fmt_name((AVPixelFormat)input->format);
        throw std::runtime_error(buf.str());
    }

    auto outContainer = new VideoFrame();
    auto output = outContainer->pointer();
    output->format = AV_PIX_FMT_YUV420P;

    if (av_hwframe_transfer_data(output, input, 0) < 0) {
        fail();
        throw std::runtime_error("Unable to extract data from VAAPI frame");
    }

    if (av_frame_copy_props(output, input) < 0 ) {
        av_frame_unref(output);
    }

    av_frame_unref(input);
    av_frame_move_ref(input, output);

    reset();
    return true;
}

void
VaapiAccel::init(AVCodecContext* codecCtx)
{
    vaProfile_ = VAProfileNone;
    vaEntryPoint_ = VAEntrypointVLD;
    using ProfileMap = std::map<int, VAProfile>;
    ProfileMap h264 = {
        { FF_PROFILE_H264_CONSTRAINED_BASELINE, VAProfileH264ConstrainedBaseline },
        { FF_PROFILE_H264_BASELINE, VAProfileH264Baseline },
        { FF_PROFILE_H264_MAIN, VAProfileH264Main },
        { FF_PROFILE_H264_HIGH, VAProfileH264High }
    };
    ProfileMap mpeg4 = {
        { FF_PROFILE_MPEG4_SIMPLE, VAProfileMPEG4Simple },
        { FF_PROFILE_MPEG4_ADVANCED_SIMPLE, VAProfileMPEG4AdvancedSimple },
        { FF_PROFILE_MPEG4_MAIN, VAProfileMPEG4Main }
    };
    ProfileMap h263 = {
        { FF_PROFILE_UNKNOWN, VAProfileH263Baseline }
    };

    std::map<int, ProfileMap> profileMap = {
        { AV_CODEC_ID_H264, h264 },
        { AV_CODEC_ID_MPEG4, mpeg4 },
        { AV_CODEC_ID_H263, h263 },
        { AV_CODEC_ID_H263P, h263 } // no clue if this'll work
    };

    VAStatus status;

#ifdef HAVE_VAAPI_ACCEL_DRM
    const char* deviceName = "/dev/dri/card0"; // check for renderDX first?
#else
    const char* deviceName = nullptr; // use default device
#endif

    AVBufferRef* hardwareDeviceCtx;
    if (av_hwdevice_ctx_create(&hardwareDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, deviceName, nullptr, 0) < 0) {
        throw std::runtime_error("Failed to create VAAPI device");
    }

    deviceBufferRef_ = av_buffer_ref(hardwareDeviceCtx);
    auto device = (AVHWDeviceContext*)deviceBufferRef_->data;
    vaConfig_ = VA_INVALID_ID;
    vaContext_ = VA_INVALID_ID;
    auto hardwareContext = (AVVAAPIDeviceContext*)device->hwctx;

    int numProfiles = vaMaxNumProfiles(hardwareContext->display);
    VAProfile* profileList = new VAProfile[numProfiles * sizeof(VAProfile)];
    try  {
        status = vaQueryConfigProfiles(hardwareContext->display, profileList, &numProfiles);
        if (status != VA_STATUS_SUCCESS) {
            std::stringstream buf;
            buf << "Failed to query profiles: " << vaErrorStr(status);
            throw std::runtime_error(buf.str());
        }

        VAProfile profile;
        if (profileMap.find(codecCtx->codec_id) != profileMap.end()) {
            auto innerMap = profileMap[codecCtx->codec_id];
            if (innerMap.find(codecCtx->profile) != innerMap.end()) {
                profile = innerMap[codecCtx->profile];
            }
        }
        for (int i = 0; i < numProfiles; i++) {
            if (profile == profileList[i]) {
                vaProfile_ = profile;
                break;
            }
        }
    } catch (const std::runtime_error& e) {
        RING_ERR("%s", e.what());
        delete[] profileList;
    }

    if (vaProfile_ == VAProfileNone)
        throw std::runtime_error("VAAPI does not support selected codec");

    status = vaCreateConfig(hardwareContext->display, vaProfile_, vaEntryPoint_, 0, 0, &vaConfig_);
    if (status != VA_STATUS_SUCCESS) {
        std::stringstream buf;
        buf << "Failed to create VAAPI configuration: " << vaErrorStr(status);
        throw std::runtime_error(buf.str());
    }

    auto hardwareConfig = (AVVAAPIHWConfig*)av_hwdevice_hwconfig_alloc(deviceBufferRef_);
    hardwareConfig->config_id = vaConfig_;

    AVHWFramesConstraints* constraints;
    try {
        constraints = av_hwdevice_get_hwframe_constraints(deviceBufferRef_, hardwareConfig);
        if (width_ < constraints->min_width
            || width_ > constraints->max_width
            || height_ < constraints->min_height
            || height_ > constraints->max_height) {
            std::stringstream buf;
            buf << "Hardware does not support image size with VAAPI: " << width_ << "x" << height_;
            throw std::runtime_error(buf.str());
        }
    } catch (const std::runtime_error& e) {
        RING_ERR("%s", e.what());
        av_hwframe_constraints_free(&constraints);
        av_freep(&hardwareConfig);
    }

    int numSurfaces = 16; // based on codec instead?
    if (codecCtx->active_thread_type & FF_THREAD_FRAME)
        numSurfaces += codecCtx->thread_count; // need extra surface per thread

    framesBufferRef_ = av_hwframe_ctx_alloc(deviceBufferRef_);
    auto frames = (AVHWFramesContext*)framesBufferRef_->data;
    frames->format = AV_PIX_FMT_VAAPI;
    frames->sw_format = AV_PIX_FMT_YUV420P;
    frames->width = width_;
    frames->height = height_;
    frames->initial_pool_size = numSurfaces;

    if (av_hwframe_ctx_init(framesBufferRef_) < 0) {
        throw std::runtime_error("Failed to initialize VAAPI frame context");
    }

    auto framesContext = (AVVAAPIFramesContext*)frames->hwctx;
    status = vaCreateContext(hardwareContext->display, vaConfig_, width_, height_,
        VA_PROGRESSIVE, framesContext->surface_ids, framesContext->nb_surfaces, &vaContext_);
    if (status != VA_STATUS_SUCCESS) {
        std::stringstream buf;
        buf << "Failed to create VAAPI context: " << vaErrorStr(status);
        throw std::runtime_error(buf.str());
    }

    RING_DBG("VAAPI decoder initialized");

    ffmpegAccelCtx_.display = hardwareContext->display;
    ffmpegAccelCtx_.config_id = vaConfig_;
    ffmpegAccelCtx_.context_id = vaContext_;
    codecCtx->hwaccel_context = (void*)&ffmpegAccelCtx_;
}

}}

#endif // defined(RING_VIDEO) && defined(RING_ACCEL)
