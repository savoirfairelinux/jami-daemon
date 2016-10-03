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

#include <string>
#include <sstream>
#include <array>

#include "video/osxvideo/videotoolbox.h"
#include "video/accel.h"

#include "logger.h"

namespace ring { namespace video {

VideoToolboxAccel::VideoToolboxAccel(AccelInfo info) : HardwareAccel(info)
{
}

VideoToolboxAccel::~VideoToolboxAccel()
{
    if (codecCtx_)
        av_videotoolbox_default_free(codecCtx_);
}

int
VideoToolboxAccel::allocateBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags)
{
    // do nothing, as this is done during extractData for VideoToolbox
    return 0;
}

bool
VideoToolboxAccel::extractData(AVCodecContext* codecCtx, VideoFrame& container)
{
    try {
        auto input = container.pointer();

        if (input->format != format_) {
            std::stringstream buf;
            buf << "Frame format mismatch: expected " << av_get_pix_fmt_name(format_);
            buf << ", got " << av_get_pix_fmt_name((AVPixelFormat)input->format);
            throw std::runtime_error(buf.str());
        }

        auto pixelBuffer = reinterpret_cast<CVPixelBufferRef>(input->data[3]);
        auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
        auto outContainer = new VideoFrame();
        auto output = outContainer->pointer();

        switch (pixelFormat) {
            case kCVPixelFormatType_420YpCbCr8Planar:
                output->format = AV_PIX_FMT_YUV420P;
                break;
            case kCVPixelFormatType_32BGRA:
                output->format = AV_PIX_FMT_BGRA;
                break;
            case kCVPixelFormatType_422YpCbCr8:
                output->format = AV_PIX_FMT_UYVY422;
                break;
            case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: // OS X 10.7+
                output->format = AV_PIX_FMT_NV12;
                break;
            default:
                char codecTag[32];
                av_get_codec_tag_string(codecTag, sizeof(codecTag), codecCtx->codec_tag);
                std::stringstream buf;
                buf << "VideoToolbox (" << codecTag << "): unsupported pixel format (";
                buf << av_get_pix_fmt_name(format_) << ")";
                throw std::runtime_error(buf.str());
        }

        output->width = input->width;
        output->height = input->height;
        // align on 32 bytes
        if (av_frame_get_buffer(output, 32) < 0) {
            throw std::runtime_error("Could not allocate a buffer for VideoToolbox");
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

        av_image_copy(output->data, output->linesize,
            const_cast<const uint8_t**>(static_cast<uint8_t**>(buffer.data())),
            lineSize.data(), static_cast<AVPixelFormat>(output->format),
            input->width, input->height);

        if (av_frame_copy_props(output, input) < 0) {
            av_frame_unref(output);
        }

        CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

        av_frame_unref(input);
        av_frame_move_ref(input, output);
    } catch (const std::runtime_error& e) {
        fail();
        RING_ERR("%s", e.what());
        return false;
    }

    succeed();
    return true;
}

bool
VideoToolboxAccel::init(AVCodecContext* codecCtx)
{
    codecCtx_ = codecCtx; // need this for destructor
    if (int ret = av_videotoolbox_default_init(codecCtx) < 0) {
        char buffer[128];
        av_strerror(AVERROR(ret), buffer, sizeof(buffer));
        RING_ERR("Could not create VideoToolbox decoder: %s", buffer);
        return false;
    }

    return true;
}

}}

#endif // defined(RING_VIDEO) && defined(RING_ACCEL)
