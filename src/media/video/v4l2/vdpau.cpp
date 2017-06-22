/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
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

#ifdef RING_VDPAU

#include "video/v4l2/vdpau.h"
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

VdpauAccel::VdpauAccel(const std::string name, const AVPixelFormat format)
    : HardwareAccel(name, format)
    , deviceBufferRef_(nullptr, avBufferRefDeleter)
    , framesBufferRef_(nullptr, avBufferRefDeleter)
{
}

VdpauAccel::~VdpauAccel()
{
}

int
VdpauAccel::allocateBuffer(AVFrame* frame, int flags)
{
    (void) flags;
    return av_hwframe_get_buffer(framesBufferRef_.get(), frame, 0);
}

void
VdpauAccel::extractData(VideoFrame& input, VideoFrame& output)
{
    auto inFrame = input.pointer();
    auto outFrame = output.pointer();

    if (av_hwframe_transfer_data(outFrame, inFrame, 0) < 0) {
        throw std::runtime_error("Unable to extract data from VDPAU frame");
    }

    if (av_frame_copy_props(outFrame, inFrame) < 0 ) {
        av_frame_unref(outFrame);
    }
}

bool
VdpauAccel::checkAvailability()
{
    AVBufferRef* hardwareDeviceCtx;
    if (av_hwdevice_ctx_create(&hardwareDeviceCtx, AV_HWDEVICE_TYPE_VDPAU, nullptr, nullptr, 0) == 0) {
        deviceBufferRef_.reset(hardwareDeviceCtx);
        return true;
    }

    av_buffer_unref(&hardwareDeviceCtx);
    return false;
}

bool
VdpauAccel::init()
{
    auto device = reinterpret_cast<AVHWDeviceContext*>(deviceBufferRef_->data);
    auto hardwareContext = static_cast<AVVDPAUDeviceContext*>(device->hwctx);

    framesBufferRef_.reset(av_hwframe_ctx_alloc(deviceBufferRef_.get()));
    auto frames = reinterpret_cast<AVHWFramesContext*>(framesBufferRef_->data);
    frames->format = AV_PIX_FMT_VDPAU;
    frames->sw_format = AV_PIX_FMT_YUV420P;
    frames->width = width_;
    frames->height = height_;

    if (av_hwframe_ctx_init(framesBufferRef_.get()) < 0) {
        RING_ERR("Failed to initialize VDPAU frame context");
        return false;
    }

    if (av_vdpau_bind_context(codecCtx_, hardwareContext->device, hardwareContext->get_proc_address, 0)) {
        RING_ERR("Could not bind VDPAU context");
        return false;
    }

    RING_DBG("VDPAU decoder initialized");

    return true;
}

}} // namespace ring::video

#endif // RING_VDPAU
