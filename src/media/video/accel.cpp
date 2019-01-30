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
    AVPixelFormat swFormat;
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

HardwareAccel::HardwareAccel(AVCodecID id, const std::string& name, AVPixelFormat format, AVPixelFormat swFormat)
    : id_(id)
    , name_(name)
    , format_(format)
    , swFormat_(swFormat)
{}

HardwareAccel::~HardwareAccel()
{
    if (deviceCtx_)
        av_buffer_unref(&deviceCtx_);
    if (framesCtx_)
        av_buffer_unref(&framesCtx_);
}

std::unique_ptr<VideoFrame>
HardwareAccel::transfer(const VideoFrame& frame)
{
    int ret = 0;
    if (type_ == CODEC_DECODER) {
        auto input = frame.pointer();
        if (input->format != format_) {
            RING_ERR() << "Frame format mismatch: expected "
                << av_get_pix_fmt_name(format_) << ", got "
                << av_get_pix_fmt_name(static_cast<AVPixelFormat>(input->format));
            return nullptr;
        }

        auto framePtr = std::make_unique<VideoFrame>();
        auto swFrame = framePtr->pointer();
        swFrame->format = swFormat_;

        if ((ret = av_hwframe_transfer_data(swFrame, input, 0)) < 0) {
            RING_ERR() << "Failed to retrieve frame from GPU: " << libav_utils::getError(ret);
            return nullptr;
        }

        swFrame->pts = input->pts; // transfer does not copy timestamp
        return framePtr;
    } else if (type_ == CODEC_ENCODER) {
        auto input = frame.pointer();
        if (input->format != swFormat_) {
            RING_ERR() << "Frame format mismatch: expected "
                << av_get_pix_fmt_name(swFormat_) << ", got "
                << av_get_pix_fmt_name(static_cast<AVPixelFormat>(input->format));
            return nullptr;
        }

        auto framePtr = std::make_unique<VideoFrame>();
        auto hwFrame = framePtr->pointer();
        hwFrame->format = format_;

        if ((ret = av_hwframe_get_buffer(framesCtx_, framePtr->pointer(), 0)) < 0
            || !hwFrame->hw_frames_ctx) {
            RING_ERR() << "Failed to allocate hardware buffer: " << libav_utils::getError(ret);
        }

        if ((ret = av_hwframe_transfer_data(hwFrame, input, 0)) < 0) {
            RING_ERR() << "Failed to push frame to GPU: " << libav_utils::getError(ret);
            return nullptr;
        }

        hwFrame->pts = input->pts; // transfer does not copy timestamp
        return framePtr;
    } else {
        RING_ERR() << "Invalid hardware accelerator";
        return nullptr;
    }
}

void
HardwareAccel::setDetails(AVCodecContext* codecCtx, AVDictionary** /*d*/)
{
    if (type_ == CODEC_DECODER) {
        codecCtx->hw_device_ctx = av_buffer_ref(deviceCtx_);
        codecCtx->get_format = getFormatCb;
        codecCtx->thread_safe_callbacks = 1;
    } else if (type_ == CODEC_ENCODER) {
        codecCtx->hw_device_ctx = av_buffer_ref(deviceCtx_);
        codecCtx->hw_frames_ctx = av_buffer_ref(framesCtx_);
    }
}

bool
HardwareAccel::initDevice()
{
    int ret = 0;
    auto hwType = av_hwdevice_find_type_by_name(name_.c_str());
#ifdef HAVE_VAAPI_ACCEL_DRM
    // default DRM device may not work on multi GPU computers, so check all possible values
    if (name_ == "vaapi") {
        const std::string path = "/dev/dri/";
        auto files = ring::fileutils::readDirectory(path);
        // renderD* is preferred over card*
        std::sort(files.rbegin(), files.rend());
        for (auto& entry : files) {
            std::string deviceName = path + entry;
            if ((ret = av_hwdevice_ctx_create(&deviceCtx_, hwType, deviceName.c_str(), nullptr, 0)) >= 0) {
                return true;
            }
        }
    }
#endif
    // default device (nullptr) works for most cases
    ret = av_hwdevice_ctx_create(&deviceCtx_, hwType, nullptr, nullptr, 0);
    return ret >= 0;
}

bool
HardwareAccel::initFrame(int width, int height)
{
    int ret = 0;
    if (!deviceCtx_) {
        RING_ERR() << "Cannot initialize hardware frames without a valid hardware device";
        return false;
    }

    framesCtx_ = av_hwframe_ctx_alloc(deviceCtx_);
    if (!framesCtx_)
        return false;

    auto ctx = reinterpret_cast<AVHWFramesContext*>(framesCtx_->data);
    ctx->format = format_;
    ctx->sw_format = swFormat_;
    ctx->width = width;
    ctx->height = height;
    ctx->initial_pool_size = 20; // TODO try other values

    if ((ret = av_hwframe_ctx_init(framesCtx_)) < 0)
        av_buffer_unref(&framesCtx_);

    return ret >= 0;
}

std::unique_ptr<HardwareAccel>
HardwareAccel::setupDecoder(AVCodecID id)
{
    static const HardwareAPI apiList[] = {
        { "vaapi", AV_PIX_FMT_VAAPI, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_VP8, AV_CODEC_ID_MJPEG } },
        { "vdpau", AV_PIX_FMT_VDPAU, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4 } },
        { "videotoolbox", AV_PIX_FMT_VIDEOTOOLBOX, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4 } },
    };

    for (const auto& api : apiList) {
        if (std::find(api.supportedCodecs.begin(), api.supportedCodecs.end(), id) != api.supportedCodecs.end()) {
            auto accel = std::make_unique<HardwareAccel>(id, api.name, api.format, api.swFormat);
            if (accel->initDevice()) {
                accel->type_ = CODEC_DECODER;
                RING_DBG() << "Attempting to use hardware decoding with " << api.name;
                return accel;
            }
        }
    }

    return nullptr;
}

std::unique_ptr<HardwareAccel>
HardwareAccel::setupEncoder(AVCodecID id, int width, int height)
{
    static const HardwareAPI apiList[] = {
        { "vaapi", AV_PIX_FMT_VAAPI, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MJPEG, AV_CODEC_ID_VP8 } },
        { "videotoolbox", AV_PIX_FMT_VIDEOTOOLBOX, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264 } },
    };

    for (auto api : apiList) {
        const auto& it = std::find(api.supportedCodecs.begin(), api.supportedCodecs.end(), id);
        if (it != api.supportedCodecs.end()) {
            std::string codecName = std::string(avcodec_get_name(*it)) + "_" + api.name;
            auto accel = std::make_unique<HardwareAccel>(id, api.name, api.format, api.swFormat);
            accel->type_ = CODEC_ENCODER;
            if (accel->initDevice() && accel->initFrame(width, height)) {
                RING_DBG() << "Attempting to use hardware encoding with " << api.name;
                return accel;
            }
        }
    }

    return nullptr;
}

}} // namespace ring::video
