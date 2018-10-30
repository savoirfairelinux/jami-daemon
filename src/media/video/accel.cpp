/*
 *  Copyright (C) 2016-2018 Savoir-faire Linux Inc.
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
            // hardware tends to under-report supported levels
            codecCtx->hwaccel_flags |= AV_HWACCEL_FLAG_IGNORE_LEVEL;
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

    // FFmpeg requires a second frame in which to transfer the data from the GPU buffer to the main memory
    auto container = std::unique_ptr<VideoFrame>(new VideoFrame());
    auto output = container->pointer();

    auto pts = input->pts;
    // most hardware accelerations output NV12, so skip extra conversions
    output->format = AV_PIX_FMT_NV12;
    int ret = av_hwframe_transfer_data(output, input, 0);
    output->pts = pts;

    // move output into input so the caller receives extracted image data
    // but we have to delete input's data first
    av_frame_unref(input);
    av_frame_move_ref(input, output);

    return ret;
}

static int
initDevice(HardwareAccel* accel, AVCodecContext* codecCtx)
{
    int ret = 0;
    AVBufferRef* hardwareDeviceCtx = nullptr;
#ifdef HAVE_VAAPI_ACCEL_DRM
    // default DRM device may not work on multi GPU computers, so check all possible values
    if (accel->name == "vaapi") {
        const std::string path = "/dev/dri/";
        auto files = ring::fileutils::readDirectory(path);
        // renderD* is preferred over card* so sort in reverse alphabetical
        std::sort(files.rbegin(), files.rend());
        for (auto& entry : files) {
            std::string deviceName = path + entry;
            if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, accel->type, deviceName.c_str(), nullptr, 0)) >= 0) {
                codecCtx->hw_device_ctx = hardwareDeviceCtx;
                accel->info = deviceName;
                return ret;
            }
        }
    }
#endif
    // default device (nullptr) works for most cases
    if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, accel->type, nullptr, nullptr, 0)) >= 0) {
        codecCtx->hw_device_ctx = hardwareDeviceCtx;
        accel->info = "default device";
    }

    return ret;
}

const HardwareAccel
setupHardwareDecoding(AVCodecContext* codecCtx)
{
    bool success = false;
    HardwareAccel accel;
    for (int i = 0;; ++i) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(codecCtx->codec, i);
        if (!config)
            break; // no acceleration for this codec

        accel.name = av_hwdevice_get_type_name(config->device_type);
        accel.format = config->pix_fmt;
        accel.type = config->device_type;
        accel.info = "";

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
            if (initDevice(&accel, codecCtx) >= 0) {
                success = true;
                break;
            }
        } else if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX) {
            // TODO
        } else if (config->methods & AV_CODEC_HW_CONFIG_METHOD_INTERNAL) {
            // nothing to do
            success = true;
            break;
        }
    }

    if (success) {
        RING_DBG() << "Using hardware decoding for " << codecCtx->codec->name
            << " with " << accel.info;
        codecCtx->get_format = getFormatCb;
        codecCtx->thread_safe_callbacks = 1;
        return accel;
    } else {
        RING_WARN() << "Not using hardware decoding for " << codecCtx->codec->name;
        return {};
    }
}

}} // namespace ring::video
