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

#if defined(RING_VIDEOTOOLBOX) || defined(RING_VDA)

#include <string>
#include <sstream>
#include <array>

#include "video/osxvideo/videotoolbox.h"
#include "video/accel.h"

#include "logger.h"

namespace ring { namespace video {

VideoToolboxAccel::VideoToolboxAccel(const std::string name, const AVPixelFormat format)
    : HardwareAccel(name, format)
{
}

VideoToolboxAccel::~VideoToolboxAccel()
{
    if (codecCtx_) {
        if (usingVT_) {
#ifdef RING_VIDEOTOOLBOX
            av_videotoolbox_default_free(codecCtx_);
#endif
        } else {
#ifdef RING_VDA
            av_vda_default_free(codecCtx_);
#endif
        }
    }
}

int
VideoToolboxAccel::allocateBuffer(AVFrame* frame, int flags)
{
    // do nothing, as this is done during extractData for VideoT and VDA
    (void) frame; // unused
    (void) flags; // unused
    return 0;
}

void
VideoToolboxAccel::extractData(VideoFrame& input, VideoFrame& output)
{
    auto inFrame = input.pointer();
    auto outFrame = output.pointer();
    auto pixelBuffer = reinterpret_cast<CVPixelBufferRef>(inFrame->data[3]);
    auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);

    switch (pixelFormat) {
        case kCVPixelFormatType_420YpCbCr8Planar:
            outFrame->format = AV_PIX_FMT_YUV420P;
            break;
        case kCVPixelFormatType_32BGRA:
            outFrame->format = AV_PIX_FMT_BGRA;
            break;
        case kCVPixelFormatType_422YpCbCr8:
            outFrame->format = AV_PIX_FMT_UYVY422;
            break;
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: // OS X 10.7+
            outFrame->format = AV_PIX_FMT_NV12;
            break;
        default:
            char codecTag[32];
            av_get_codec_tag_string(codecTag, sizeof(codecTag), codecCtx_->codec_tag);
            std::stringstream buf;
            buf << decoderName_ << " (" << codecTag << "): unsupported pixel format (";
            buf << av_get_pix_fmt_name(format_) << ")";
            throw std::runtime_error(buf.str());
    }

    outFrame->width = inFrame->width;
    outFrame->height = inFrame->height;
    // align on 32 bytes
    if (av_frame_get_buffer(outFrame, 32) < 0) {
        std::stringstream buf;
        buf << "Could not allocate a buffer for " << decoderName_;
        throw std::runtime_error(buf.str());
    }

    if (CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) {
        throw std::runtime_error("Could not lock the pixel buffer");
    }

    // av_image_copy function takes a 4 element array (according to its signature)
    std::array<uint8_t*, 4> buffer = {};
    std::array<int, 4> lineSize = {};
    if (CVPixelBufferIsPlanar(pixelBuffer)) {
        int planeCount = CVPixelBufferGetPlaneCount(pixelBuffer);
        for (int i = 0; i < planeCount; i++) {
            buffer[i] = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, i));
            lineSize[i] = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, i);
        }
    } else {
        buffer[0] = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer));
        lineSize[0] = CVPixelBufferGetBytesPerRow(pixelBuffer);
    }

    av_image_copy(outFrame->data, outFrame->linesize,
        const_cast<const uint8_t**>(static_cast<uint8_t**>(buffer.data())),
        lineSize.data(), static_cast<AVPixelFormat>(outFrame->format),
        inFrame->width, inFrame->height);

    if (av_frame_copy_props(outFrame, inFrame) < 0) {
        av_frame_unref(outFrame);
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
}

bool
VideoToolboxAccel::checkAvailability()
{
    // VideoToolbox is always present on Mac 10.8+ and iOS 8+
    // VDA is always present on Mac 10.6.3+
    return true;
}

bool
VideoToolboxAccel::init()
{
    decoderName_ = "";
    bool success = false;
#ifdef RING_VIDEOTOOLBOX
    if (int ret = av_videotoolbox_default_init(codecCtx_) == 0) {
        success = true;
        usingVT_ = true;
        decoderName_ = "VideoToolbox";
    }
#endif
#ifdef RING_VDA
    if (!success) {
        if (int ret = av_vda_default_init(codecCtx_) == 0) {
            success = true;
            usingVT_ = false;
            decoderName_ = "VDA";
        }
    }
#endif

    if (success)
        RING_DBG("%s decoder initialized", decoderName_.c_str());
    else
        RING_ERR("Failed to initialize Mac hardware accelerator");

    return success;
}

}}

#endif // defined(RING_VIDEOTOOLBOX) || defined(RING_VDA)
