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

extern "C" {
#include <libavutil/hwcontext.h>
}

#include <algorithm>

#include "media_buffer.h"
#include "string_utils.h"
#include "fileutils.h"
#include "logger.h"
#include "accel.h"

namespace ring { namespace video {

static AVPixelFormat
getFormatCb(AVCodecContext* codecCtx, const AVPixelFormat* formats)
{
    auto accel = static_cast<HardwareAccel*>(codecCtx->opaque);

    AVPixelFormat fallback = AV_PIX_FMT_NONE;
    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; ++i) {
        fallback = formats[i];
        if (formats[i] == accel->format) {
            RING_DBG("Found hardware format %s for %s acceleration", av_get_pix_fmt_name(formats[i]), accel->name.c_str());
            return formats[i];
        }
    }

    accel->name = {}; // don't use accel
    RING_WARN("No hardware decoding possible, will decode to %s", av_get_pix_fmt_name(fallback));
    return fallback;
}

int
transferFrameData(HardwareAccel accel, AVCodecContext* codecCtx, VideoFrame& frame)
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

    // FFmpeg requires a second frame in which to transfer the data from the GPU buffer to the main memory
    auto container = std::unique_ptr<VideoFrame>(new VideoFrame());
    auto output = container->pointer();
    output->format = AV_PIX_FMT_YUV420P;
    int ret = av_hwframe_transfer_data(output, input, 0);

    // move output into input so the caller receives extracted image data
    // but we have to delete input's data first
    av_frame_unref(input);
    av_frame_move_ref(input, output);

    return ret;
}

// for accelerations that do not support a default device
static std::string
getDeviceName(std::string accelName, AVBufferRef** hardwareDeviceCtx)
{
#if defined(RING_VAAPI) && defined(HAVE_VAAPI_ACCEL_DRM)
    if (accelName == "vaapi") {
        const std::string path = "/dev/dri/";
        auto files = ring::fileutils::readDirectory(path);
        // renderD* is preferred over card*
        std::sort(files.rbegin(), files.rend());
        for (auto& entry : files) {
            std::string deviceName = path + entry;
            if (av_hwdevice_ctx_create(hardwareDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, deviceName.c_str(), nullptr, 0) >= 0) {
                return deviceName;
            }
        }
    }
#endif
    return {};
}

const HardwareAccel
setupHardwareDecoding(AVCodecContext* codecCtx)
{
    const HardwareAccel accels[] = {
#ifdef RING_VAAPI
        { "vaapi", AV_PIX_FMT_VAAPI, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263 } },
#endif
#ifdef RING_VDPAU
        { "vdpau", AV_PIX_FMT_VDPAU, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263 } },
#endif
#ifdef RING_VIDEOTOOLBOX
        { "videotoolbox", AV_PIX_FMT_VIDEOTOOLBOX, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263 } },
#endif
        { "", AV_PIX_FMT_NONE, {} } // doesn't compile if there are no items in the array
    };

    AVBufferRef* hardwareDeviceCtx = nullptr;
    for (auto info : accels) {
        if (std::find(info.supportedCodecs.begin(), info.supportedCodecs.end(), static_cast<AVCodecID>(codecCtx->codec_id)) != info.supportedCodecs.end()) {
            auto hwType = av_hwdevice_find_type_by_name(info.name.c_str());
            auto deviceName = getDeviceName(info.name, &hardwareDeviceCtx);
            // prevent opening the device twice by short-circuiting if a device name was returned
            if (!deviceName.empty() || av_hwdevice_ctx_create(&hardwareDeviceCtx, hwType, deviceName.c_str(), nullptr, 0) >= 0) {
                codecCtx->hw_device_ctx = av_buffer_ref(hardwareDeviceCtx);
                codecCtx->get_format = getFormatCb;
                codecCtx->thread_safe_callbacks = 1;
                if (!deviceName.empty())
                    RING_DBG("Attempting to use '%s' hardware acceleration with device '%s'", info.name.c_str(), deviceName.c_str());
                else
                    RING_DBG("Attempting to use '%s' hardware acceleration", info.name.c_str());
                return info;
            }
        }
    }

    // TODO check for standalone hardware decoders

    RING_WARN("Not using hardware acceleration");
    return {};
}

}} // namespace ring::video
