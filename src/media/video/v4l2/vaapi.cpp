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

#include "fileutils.h"

#include <sstream>
#include <stdexcept>
#include <map>
#include <algorithm>
#include <vector>

#include "logger.h"

namespace ring { namespace video {

static auto avBufferRefDeleter = [](AVBufferRef* buf){ av_buffer_unref(&buf); };

VaapiAccel::VaapiAccel(AccelInfo info) : HardwareAccel(info)
    , deviceBufferRef_(nullptr, avBufferRefDeleter)
    , framesBufferRef_(nullptr, avBufferRefDeleter)
{
}

VaapiAccel::~VaapiAccel()
{
}

int
VaapiAccel::allocateBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags)
{
    return av_hwframe_get_buffer(framesBufferRef_.get(), frame, 0);
}

bool
VaapiAccel::extractData(AVCodecContext* codecCtx, VideoFrame& container)
{
    try {
        auto input = container.pointer();

        if (input->format != format_) {
            std::stringstream buf;
            buf << "Frame format mismatch: expected " << av_get_pix_fmt_name(format_);
            buf << ", got " << av_get_pix_fmt_name((AVPixelFormat)input->format);
            throw std::runtime_error(buf.str());
        }

        auto outContainer = new VideoFrame();
        auto output = outContainer->pointer();
        output->format = AV_PIX_FMT_YUV420P;

        if (av_hwframe_transfer_data(output, input, 0) < 0) {
            throw std::runtime_error("Unable to extract data from VAAPI frame");
        }

        if (av_frame_copy_props(output, input) < 0 ) {
            av_frame_unref(output);
        }

        av_frame_unref(input);
        av_frame_move_ref(input, output);
    } catch (const std::runtime_error& e) {
        fail(codecCtx, false);
        RING_ERR("%s", e.what());
        return false;
    }

    succeed();
    return true;
}

bool
VaapiAccel::init(AVCodecContext* codecCtx)
{
#ifdef HAVE_VAAPI_ACCEL_DRM
    // try all possible devices, use first one that works
    const std::string path = "/dev/dri/";
    for (auto& entry : ring::fileutils::readDirectory(path)) {
        // a drm device is either a card or a render node, check both
        const std::string prefixCard = "card";
        if (!entry.compare(0, prefixCard.size(), prefixCard.c_str()))
            if (open(codecCtx, path + entry))
                return true;

        const std::string prefixNode = "renderD";
        if (!entry.compare(0, prefixNode.size(), prefixNode.c_str()))
            if (open(codecCtx, path + entry))
                return true;
    }
    return false;
#elif HAVE_VAAPI_ACCEL_X11
    return open(codecCtx, ":0"); // this is the default x11 device
#endif
}

bool
VaapiAccel::open(AVCodecContext* codecCtx, std::string deviceName)
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
        { AV_CODEC_ID_H263P, h263 }
    };

    VAStatus status;
    AVBufferRef* hardwareDeviceCtx;
    if (av_hwdevice_ctx_create(&hardwareDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, deviceName.c_str(), nullptr, 0) < 0) {
        RING_ERR("Failed to create VAAPI device using %s", deviceName.c_str());
        av_buffer_unref(&hardwareDeviceCtx);
        return false;
    }

    deviceBufferRef_.reset(av_buffer_ref(hardwareDeviceCtx));

    auto device = reinterpret_cast<AVHWDeviceContext*>(deviceBufferRef_->data);
    vaConfig_ = VA_INVALID_ID;
    vaContext_ = VA_INVALID_ID;
    auto hardwareContext = static_cast<AVVAAPIDeviceContext*>(device->hwctx);

    int numProfiles = vaMaxNumProfiles(hardwareContext->display);
    auto profiles = std::vector<VAProfile>(numProfiles);
    status = vaQueryConfigProfiles(hardwareContext->display, profiles.data(), &numProfiles);
    if (status != VA_STATUS_SUCCESS) {
        RING_ERR("Failed to query profiles: %s", vaErrorStr(status));
        return false;
    }

    VAProfile codecProfile;
    auto itOuter = profileMap.find(codecCtx->codec_id);
    if (itOuter != profileMap.end()) {
        auto innerMap = itOuter->second;
        auto itInner = innerMap.find(codecCtx->profile);
        if (itInner != innerMap.end()) {
            codecProfile = itInner->second;
        }
    }

    auto iter = std::find_if(std::begin(profiles),
                             std::end(profiles),
                             [codecProfile](const VAProfile& p){ return p == codecProfile; });

    if (iter == std::end(profiles)) {
        RING_ERR("VAAPI does not support selected codec");
        return false;
    }

    vaProfile_ = *iter;

    status = vaCreateConfig(hardwareContext->display, vaProfile_, vaEntryPoint_, 0, 0, &vaConfig_);
    if (status != VA_STATUS_SUCCESS) {
        RING_ERR("Failed to create VAAPI configuration: %s", vaErrorStr(status));
        return false;
    }

    auto hardwareConfig = static_cast<AVVAAPIHWConfig*>(av_hwdevice_hwconfig_alloc(deviceBufferRef_.get()));
    hardwareConfig->config_id = vaConfig_;

    auto constraints = av_hwdevice_get_hwframe_constraints(deviceBufferRef_.get(), hardwareConfig);
    if (width_ < constraints->min_width
        || width_ > constraints->max_width
        || height_ < constraints->min_height
        || height_ > constraints->max_height) {
        av_hwframe_constraints_free(&constraints);
        av_freep(&hardwareConfig);
        RING_ERR("Hardware does not support image size with VAAPI: %dx%d", width_, height_);
        return false;
    }

    int numSurfaces = 16; // based on codec instead?
    if (codecCtx->active_thread_type & FF_THREAD_FRAME)
        numSurfaces += codecCtx->thread_count; // need extra surface per thread

    framesBufferRef_.reset(av_hwframe_ctx_alloc(deviceBufferRef_.get()));
    auto frames = reinterpret_cast<AVHWFramesContext*>(framesBufferRef_->data);
    frames->format = AV_PIX_FMT_VAAPI;
    frames->sw_format = AV_PIX_FMT_YUV420P;
    frames->width = width_;
    frames->height = height_;
    frames->initial_pool_size = numSurfaces;

    if (av_hwframe_ctx_init(framesBufferRef_.get()) < 0) {
        RING_ERR("Failed to initialize VAAPI frame context");
        return false;
    }

    auto framesContext = static_cast<AVVAAPIFramesContext*>(frames->hwctx);
    status = vaCreateContext(hardwareContext->display, vaConfig_, width_, height_,
        VA_PROGRESSIVE, framesContext->surface_ids, framesContext->nb_surfaces, &vaContext_);
    if (status != VA_STATUS_SUCCESS) {
        RING_ERR("Failed to create VAAPI context: %s", vaErrorStr(status));
        return false;
    }

    RING_DBG("VAAPI decoder initialized via device: %s", deviceName.c_str());

    ffmpegAccelCtx_.display = hardwareContext->display;
    ffmpegAccelCtx_.config_id = vaConfig_;
    ffmpegAccelCtx_.context_id = vaContext_;
    codecCtx->hwaccel_context = (void*)&ffmpegAccelCtx_;
    return true;
}

}}

#endif // defined(RING_VIDEO) && defined(RING_ACCEL)
