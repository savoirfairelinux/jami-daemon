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

struct HardwareAPI
{
    std::string name;
    AVPixelFormat format;
    std::vector<AVCodecID> supportedCodecs;
};

static AVPixelFormat
getFormatCb(AVCodecContext* codecCtx, const AVPixelFormat* formats)
{
    auto accel = static_cast<HardwareAccel*>(codecCtx->opaque);

    AVPixelFormat fallback = AV_PIX_FMT_NONE;
    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; ++i) {
        fallback = formats[i];
        if (accel && formats[i] == accel->getFormat()) {
            // found hardware format for codec with api
            RING_DBG() << "Found compatible hardware format for "
                << avcodec_get_name(static_cast<AVCodecID>(accel->getCodecId()))
                << " with " << accel->getName();
            return formats[i];
        }
    }

    RING_WARN() << "Not using hardware decoding";
    return fallback;
}

HardwareAccel::HardwareAccel(AVCodecID id, const std::string& name, AVPixelFormat format)
    : id_(id)
    , name_(name)
    , format_(format)
{}

std::unique_ptr<VideoFrame>
HardwareAccel::transfer(const VideoFrame& frame)
{
    auto input = frame.pointer();
    if (input->format != format_) {
        RING_ERR("Frame format mismatch: expected %s, got %s",
                 av_get_pix_fmt_name(static_cast<AVPixelFormat>(format_)),
                 av_get_pix_fmt_name(static_cast<AVPixelFormat>(input->format)));
        return nullptr;
    }

    auto framePtr = std::make_unique<VideoFrame>();
    // most hardware accelerations output NV12, so skip extra conversions
    framePtr->pointer()->format = AV_PIX_FMT_NV12;

    int ret = av_hwframe_transfer_data(framePtr->pointer(), input, 0);
    if (ret < 0) {
        RING_ERR() << "Failed to retrieve frame from GPU: " << libav_utils::getError(ret);
        return nullptr;
    }

    framePtr->pointer()->pts = input->pts; // transfer does not copy timestamp
    return framePtr;
}

static int
initDevice(const HardwareAPI& api, AVCodecContext* codecCtx)
{
    int ret = 0;
    AVBufferRef* hardwareDeviceCtx = nullptr;
    auto hwType = av_hwdevice_find_type_by_name(api.name.c_str());
#ifdef HAVE_VAAPI_ACCEL_DRM
    // default DRM device may not work on multi GPU computers, so check all possible values
    if (api.name == "vaapi") {
        const std::string path = "/dev/dri/";
        auto files = ring::fileutils::readDirectory(path);
        // renderD* is preferred over card*
        std::sort(files.rbegin(), files.rend());
        for (auto& entry : files) {
            std::string deviceName = path + entry;
            if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, hwType, deviceName.c_str(), nullptr, 0)) >= 0) {
                codecCtx->hw_device_ctx = hardwareDeviceCtx;
                return ret;
            }
        }
    }
#endif
    // default device (nullptr) works for most cases
    if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, hwType, nullptr, nullptr, 0)) >= 0) {
        codecCtx->hw_device_ctx = hardwareDeviceCtx;
    }

    return ret;
}

std::unique_ptr<HardwareAccel>
HardwareAccel::setupDecoder(AVCodecContext* codecCtx)
{
    const HardwareAPI apiList[] = {
        { "vaapi", AV_PIX_FMT_VAAPI, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_VP8, AV_CODEC_ID_MJPEG } },
        { "vdpau", AV_PIX_FMT_VDPAU, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4 } },
        { "videotoolbox", AV_PIX_FMT_VIDEOTOOLBOX, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4 } },
    };

    for (auto api : apiList) {
        if (std::find(api.supportedCodecs.begin(), api.supportedCodecs.end(), codecCtx->codec_id) != api.supportedCodecs.end()) {
            if (initDevice(api, codecCtx) >= 0) {
                codecCtx->get_format = getFormatCb;
                codecCtx->thread_safe_callbacks = 1;
                RING_DBG() << "Attempting to use hardware accelerated decoding with " << api.name;
                return std::make_unique<HardwareAccel>(codecCtx->codec_id, api.name, api.format);
            }
        }
    }

    return nullptr;
}

}} // namespace ring::video
