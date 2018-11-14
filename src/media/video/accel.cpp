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
        if (accel && formats[i] == accel->format)
            return formats[i];
    }

    if (accel) {
        RING_WARN() << accel->name << " acceleration not supported, falling back to software decoding";
        accel->name = {}; // don't use accel
    } else {
        RING_WARN() << "Not using hardware decoding";
    }
    return fallback;
}

int
transferFrameDecode(HardwareAccel accel, AVCodecContext* /*codecCtx*/, VideoFrame& frame)
{
    if (accel.name.empty())
        return -1;

    auto input = frame.pointer();
    if (input->format != accel.format) {
        RING_ERR() << "Frame format mismatch: expected " <<
                av_get_pix_fmt_name(static_cast<AVPixelFormat>(accel.format)) <<
                ", got " <<
                av_get_pix_fmt_name(static_cast<AVPixelFormat>(input->format));
        return -1;
    }

    // FFmpeg requires a second frame in which to transfer the data from the GPU buffer to the main memory
    auto container = std::unique_ptr<VideoFrame>(new VideoFrame());
    auto output = container->pointer();

    auto pts = input->pts;
    output->format = accel.swFormat;
    int ret = av_hwframe_transfer_data(output, input, 0);
    output->pts = pts;

    // move output into input so the caller receives extracted image data
    // but we have to delete input's data first
    av_frame_unref(input);
    av_frame_move_ref(input, output);

    return ret;
}

int
transferFrameEncode(HardwareAccel accel, AVCodecContext* enc, VideoFrame& frame)
{
    int ret = 0;
    VideoFrame v1;
    AVFrame *hwFrame = NULL;

    auto input = frame.pointer();
    hwFrame = v1.pointer();

    if ((ret = av_hwframe_get_buffer(enc->hw_frames_ctx, hwFrame, 0)) < 0) {
        RING_ERR() << "Error code: " << libav_utils::getError(ret) << " ." << __LINE__;
    }

    if (!hwFrame->hw_frames_ctx) {
        ret = AVERROR(ENOMEM);
    }

    if ((ret = av_hwframe_transfer_data(hwFrame, input, 0)) < 0) {
        RING_ERR() << "Error while transferring frame data to surface."
                "Error code: %s." << libav_utils::getError(ret);
    }

    hwFrame->pts = input->pts;

    av_frame_unref(input);
    av_frame_move_ref(input, hwFrame);

    return ret;
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
                RING_DBG() << "Using '" << accel.name << "' hardware acceleration with device '" << deviceName << "'";
                return ret;
            }
        }
    }
#endif
    // default device (nullptr) works for most cases
    if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, hwType, nullptr, nullptr, 0)) >= 0) {
        codecCtx->hw_device_ctx = hardwareDeviceCtx;
        RING_DBG() << "Using '" << accel.name << "' hardware acceleration";
    }

    return ret;
}

static int
initHwFrameCtx(HardwareAccel& accel, AVCodecContext *ctx)
{
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = NULL;
    int err = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(ctx->hw_device_ctx))) {
        RING_ERR() << "Failed to create VAAPI frame context.";
        return -1;
    }

    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = accel.format;
    frames_ctx->sw_format = accel.swFormat;
    frames_ctx->width     = ctx->width;
    frames_ctx->height    = ctx->height;
    frames_ctx->initial_pool_size = 20; //taken from ffmpeg example code

    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        RING_ERR() << "Failed to initialize hardware frame context." <<
                "Error code: " << libav_utils::getError(err);
        av_buffer_unref(&hw_frames_ref);
        return err;
    }

    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);

    if (!ctx->hw_frames_ctx)
        err = AVERROR(ENOMEM);

    av_buffer_unref(&hw_frames_ref);

    return err;
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
        { "vaapi", true, AV_PIX_FMT_VAAPI, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263, AV_CODEC_ID_VP8, AV_CODEC_ID_MJPEG } },
        { "vdpau", true, AV_PIX_FMT_VDPAU, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263 } },
        { "videotoolbox", true, AV_PIX_FMT_VIDEOTOOLBOX, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263 } },
    };

    for (auto accel : accels) {
        if (std::find(accel.supportedCodecs.begin(), accel.supportedCodecs.end(),
                static_cast<AVCodecID>(codecCtx->codec_id)) != accel.supportedCodecs.end()) {
			RING_WARN() << "Found decoder with codec " << avcodec_get_name(codecCtx->codec_id) << " and API " << accel.name << "attempt to initialize the device.";

            if (initDevice(accel, codecCtx) >= 0) {
                codecCtx->get_format = getFormatCb;
                codecCtx->thread_safe_callbacks = 1;
                RING_WARN() << "Using decoder with codec " << avcodec_get_name(codecCtx->codec_id) << " and API" << accel.name;
                return accel;
            }
        }
    }

    RING_WARN() << "Not using hardware accelerated decoding, falling back to software decoding.";
    return {};
}

const HardwareAccel
setupHardwareEncoding(AVCodecContext** codecCtx, AVCodec** codec)
{
    const HardwareAccel accels[] = {
        { "omx", false, AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV420P, { AV_CODEC_ID_H264 } },
        { "vaapi", true, AV_PIX_FMT_VAAPI, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_HEVC, AV_CODEC_ID_MJPEG, AV_CODEC_ID_VP8, AV_CODEC_ID_VP9 } },
        { "videotoolbox", true, AV_PIX_FMT_VIDEOTOOLBOX, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263, AV_CODEC_ID_HEVC } },
    };

    for (auto accel : accels) {
        if (std::find(accel.supportedCodecs.begin(), accel.supportedCodecs.end(),
                      static_cast<AVCodecID>((*codecCtx)->codec_id)) != accel.supportedCodecs.end()) {
            RING_WARN() << "Found encoder with codec " << avcodec_get_name((*codecCtx)->codec_id) << " and API " << accel.name << ", attempt to initialize the device.";

            for (unsigned int i = 0; i < accel.supportedCodecs.size(); i++) {
                std::string s1(avcodec_get_name(accel.supportedCodecs[i]));

                if ((*codec = avcodec_find_encoder_by_name((s1 + "_" + accel.name).c_str()))) {
                    //reprepare codeccontext for new codec
                    AVCodecContext* encoderCtx = avcodec_alloc_context3(*codec);
                    encoderCtx->width = (*codecCtx)->width;
                    encoderCtx->height = (*codecCtx)->height;
                    // satisfy ffmpeg: denominator must be 16bit or less value
                    // time base = 1/FPS
                    encoderCtx->time_base = (*codecCtx)->time_base;
                    encoderCtx->framerate = (*codecCtx)->framerate;
                    // emit one intra frame every gop_size frames
                    encoderCtx->max_b_frames = 0;
                    encoderCtx->pix_fmt = accel.format;
                    *codecCtx = encoderCtx;

                    //No need to initialize device if using OpenMAX (Raspberry Pi)
                    if (accel.requiresInitialization == false)
                        return accel;

                    if (initDevice(accel, *codecCtx) >= 0) {
                        //add init hw_frame_ctx
                        if (initHwFrameCtx(accel, *codecCtx) >= 0) {
                            (*codecCtx)->thread_safe_callbacks = 1;
                            RING_WARN() << "Using encoder with codec " << avcodec_get_name((*codecCtx)->codec_id) << " and API " << accel.name;
                            return accel;
                        }
                    }
                }
            }
        }
    }

    RING_WARN() << "Not using hardware accelerated encoding, falling back to software encoding.";
    return {};
}

}} // namespace ring::video
