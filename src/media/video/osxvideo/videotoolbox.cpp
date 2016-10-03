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
        av_videotoolbox_default_free(codecCtx);
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

        auto pixelBuffer = static_cast<CVPixelBufferRef>(frame->data[3]);
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
                buf << "VideoToolbox (" << codecTag << "): unsupported pixel format";
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

        // this code is ugly, surely there's a better way to do this
        int planeCount = CVPixelBufferGetPlaneCount(pixelBuffer); // zero if non planar
        uint8_t* data[planeCount ? planeCount : 1] = { 0 };
        int* lineSize[planeCount ? planeCount : 1] = { 0 };
        if (planeCount) {
            for (int i = 0; i < planeCount) {
                data[i] = CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, i);
                lineSize[i] = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, i);
            }
        } else {
            data[0] = CVPixelBufferGetBaseAddress(pixelBuffer);
            lineSize[0] = CVPixelBufferGetBytesPerRow(pixelBuffer);
        }

        av_image_copy(output->data, output->linesize,
            (const uint8_t**)data, lineSize, output->format,
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
    if (av_videotoolbox_default_init(codecCtx) < 0) {
        RING_ERR("Could not create VideoToolbox decoder");
        return false;
    }

    return true;
}

}}

#endif // defined(RING_VIDEO) && defined(RING_ACCEL)
