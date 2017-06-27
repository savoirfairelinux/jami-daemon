/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
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

#ifdef RING_VAAPI

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

VaapiAccel::VaapiAccel(const std::string name, const AVPixelFormat format)
    : HardwareAccel(name, format)
    , deviceBufferRef_(nullptr, avBufferRefDeleter)
    , framesBufferRef_(nullptr, avBufferRefDeleter)
{
}

VaapiAccel::~VaapiAccel()
{
}

int
VaapiAccel::allocateBuffer(AVFrame* frame, int flags)
{
    (void) flags; // unused
    return av_hwframe_get_buffer(framesBufferRef_.get(), frame, 0);
}

void
VaapiAccel::extractData(VideoFrame& input, VideoFrame& output)
{
    auto inFrame = input.pointer();
    auto outFrame = output.pointer();

    if (av_hwframe_transfer_data(outFrame, inFrame, 0) < 0) {
        throw std::runtime_error("Unable to extract data from VAAPI frame");
    }

    if (av_frame_copy_props(outFrame, inFrame) < 0 ) {
        av_frame_unref(outFrame);
    }
}

bool
VaapiAccel::checkAvailability()
{
    AVBufferRef* hardwareDeviceCtx = nullptr;
#ifdef HAVE_VAAPI_ACCEL_DRM
    const std::string path = "/dev/dri/";
    auto files = ring::fileutils::readDirectory(path);
    // renderD* is preferred over card*
    std::sort(files.rbegin(), files.rend());
    for (auto& entry : files) {
        std::string deviceName = path + entry;
        if (av_hwdevice_ctx_create(&hardwareDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, deviceName.c_str(), nullptr, 0) >= 0) {
            deviceName_ = deviceName;
            break;
        }
    }
    if (hardwareDeviceCtx == nullptr)
        return false;
#elif HAVE_VAAPI_ACCEL_X11
    if (av_hwdevice_ctx_create(&hardwareDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0) < 0) {
        return false;
    }
#endif

    deviceBufferRef_.reset(hardwareDeviceCtx);
    return true;
}

bool
VaapiAccel::init()
{
    int numSurfaces = 16; // based on codec instead?
    if (codecCtx_->active_thread_type & FF_THREAD_FRAME)
        numSurfaces += codecCtx_->thread_count; // need extra surface per thread

    framesBufferRef_.reset(av_hwframe_ctx_alloc(deviceBufferRef_.get()));
    auto frames = reinterpret_cast<AVHWFramesContext*>(framesBufferRef_->data);
    frames->format = format_;
    frames->sw_format = AV_PIX_FMT_YUV420P;
    frames->width = width_;
    frames->height = height_;
    frames->initial_pool_size = numSurfaces;

    if (av_hwframe_ctx_init(framesBufferRef_.get()) < 0) {
        RING_ERR("Failed to initialize VAAPI frame context");
        return false;
    }

    codecCtx_->hw_frames_ctx = av_buffer_ref(framesBufferRef_.get());

    if (!deviceName_.empty())
        RING_DBG("VAAPI decoder initialized via device: %s", deviceName_.c_str());
    else
        RING_DBG("VAAPI decoder initialized");
    return true;
}

}}

#endif // RING_VAAPI
