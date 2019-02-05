/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
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

extern "C" {
#include <libavutil/hwcontext.h>
}

#include <algorithm>

#include "media_buffer.h"
#include "string_utils.h"
#include "fileutils.h"
#include "logger.h"
#include "accel.h"
#include "config.h"

namespace ring { namespace video {

static AVPixelFormat
getFormatCb(AVCodecContext* codecCtx, const AVPixelFormat* formats)
{
    auto accel = static_cast<HardwareAccel*>(codecCtx->opaque);

    AVPixelFormat fallback = AV_PIX_FMT_NONE;
    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; ++i) {
        fallback = formats[i];
        if (accel && formats[i] == accel->format) {
            return formats[i];
        }
    }

    if (accel) {
        RING_WARN("'%s' acceleration not supported, falling back to software decoding", accel->name.c_str());
        accel->name = {}; // don't use accel
    } else {
        RING_WARN() << "Not using hardware decoding";
    }
    return fallback;
}

int
transferFrameData(HardwareAccel accel, AVCodecContext* /*codecCtx*/, VideoFrame& frame)
{
    if (accel.name.empty())
        return -1;

    auto input = frame.pointer();
    if (input->format != accel.format) {
        RING_ERR("Frame format mismatch: expected %s, got %s",
                 av_get_pix_fmt_name(static_cast<AVPixelFormat>(accel.format)),
                 av_get_pix_fmt_name(static_cast<AVPixelFormat>(input->format)));
        return -1;
    }

    auto output = transferToMainMemory(frame, AV_PIX_FMT_NV12);
    if (!output)
        return -1;

    frame.copyFrom(*output); // copy to input so caller receives extracted image data
    return 0;
}

std::unique_ptr<VideoFrame>
transferToMainMemory(VideoFrame& frame, AVPixelFormat desiredFormat)
{
    auto input = frame.pointer();
    auto out = std::make_unique<VideoFrame>();

    auto desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(input->format));
    if (desc && not (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        out->copyFrom(frame);
        return out;
    }

    auto output = out->pointer();
    output->format = desiredFormat;

    int ret = av_hwframe_transfer_data(output, input, 0);
    if (ret < 0) {
        out->copyFrom(frame);
        return out;
    }

    output->pts = input->pts;
    return out;
}

static int
initDevice(HardwareAccel accel, AVCodecContext* codecCtx)
{
    int ret = 0;
    AVBufferRef* hardwareDeviceCtx = nullptr;
    auto hwType = av_hwdevice_find_type_by_name(accel.name.c_str());
#ifdef HAVE_VAAPI_ACCEL_DRM
    // default DRM device may not work on multi GPU computers, so check all possible values
    if (accel.name == "vaapi") {
        const std::string path = "/dev/dri/";
        auto files = ring::fileutils::readDirectory(path);
        // renderD* is preferred over card*
        std::sort(files.rbegin(), files.rend());
        for (auto& entry : files) {
            std::string deviceName = path + entry;
            if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, hwType, deviceName.c_str(), nullptr, 0)) >= 0) {
                codecCtx->hw_device_ctx = hardwareDeviceCtx;
                RING_DBG("Using '%s' hardware acceleration with device '%s'", accel.name.c_str(), deviceName.c_str());
                return ret;
            }
        }
    }
#endif
    // default device (nullptr) works for most cases
    if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, hwType, nullptr, nullptr, 0)) >= 0) {
        codecCtx->hw_device_ctx = hardwareDeviceCtx;
        RING_DBG("Using '%s' hardware acceleration", accel.name.c_str());
    }

    return ret;
}

const HardwareAccel
setupHardwareDecoding(AVCodecContext* codecCtx)
{
    /**
     * This array represents FFmpeg's hwaccels, along with their pixel format
     * and their potentially supported codecs. Each item contains:
     * - Name (must match the name used in FFmpeg)
     * - Pixel format (tells FFmpeg which hwaccel to use)
     * - Array of AVCodecID (potential codecs that can be accelerated by the hwaccel)
     * Note: an empty name means the video isn't accelerated
     */
    const HardwareAccel accels[] = {
        { "vaapi", AV_PIX_FMT_VAAPI, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263, AV_CODEC_ID_VP8, AV_CODEC_ID_MJPEG } },
        { "vdpau", AV_PIX_FMT_VDPAU, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263 } },
        { "videotoolbox", AV_PIX_FMT_VIDEOTOOLBOX, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263 } },
    };

    for (auto accel : accels) {
        if (std::find(accel.supportedCodecs.begin(), accel.supportedCodecs.end(),
                static_cast<AVCodecID>(codecCtx->codec_id)) != accel.supportedCodecs.end()) {
            if (initDevice(accel, codecCtx) >= 0) {
                codecCtx->get_format = getFormatCb;
                codecCtx->thread_safe_callbacks = 1;
                return accel;
            }
        }
    }

    RING_WARN("Not using hardware accelerated decoding");
    return {};
}

}} // namespace ring::video
