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
    //auto hwType = av_hwdevice_find_type_by_name(accel.name.c_str());
#ifdef HAVE_VAAPI_ACCEL_DRM
    // default DRM device may not work on multi GPU computers, so check all possible values
    if (accel->name == "vaapi") {
        const std::string path = "/dev/dri/";
        auto files = ring::fileutils::readDirectory(path);
        // renderD* is preferred over card*
        std::sort(files.rbegin(), files.rend());
        for (auto& entry : files) {
            std::string deviceName = path + entry;
            //if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, hwType, deviceName.c_str(), nullptr, 0)) >= 0) {
            if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, accel->type, deviceName.c_str(), nullptr, 0)) >= 0) {
                codecCtx->hw_device_ctx = hardwareDeviceCtx;
                //RING_DBG("Using '%s' hardware acceleration with device '%s'", accel.name.c_str(), deviceName.c_str());
                return ret;
            }
        }
    }
#endif
    // default device (nullptr) works for most cases
    //if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, hwType, nullptr, nullptr, 0)) >= 0) {
    if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, accel->type, nullptr, nullptr, 0)) >= 0) {
        codecCtx->hw_device_ctx = hardwareDeviceCtx;
        accel->info = "default device";
        //RING_DBG("Using '%s' hardware acceleration", accel.name.c_str());
    }

    return ret;
}
/*
static int
initHwFrameCtx(AVCodecContext *ctx){
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = NULL;
    int err = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(ctx->hw_device_ctx))) {
        fprintf(stderr, "Failed to create VAAPI frame context.\n");
        return -1;
    }

    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_VAAPI;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width     = ctx->width;
    frames_ctx->height    = ctx->height;
    frames_ctx->initial_pool_size = 20;
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        fprintf(stderr, "Failed to initialize VAAPI frame context."
                "Error code: %s\n",libav_utils::getError(err).c_str());
        av_buffer_unref(&hw_frames_ref);
        return err;
    }
    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!ctx->hw_frames_ctx)
        err = AVERROR(ENOMEM);

    av_buffer_unref(&hw_frames_ref);
    return err;
}
*/
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
}/*
{
    //  This array represents FFmpeg's hwaccels, along with their pixel format
    //  and their potentially supported codecs. Each item contains:
    //  - Name (must match the name used in FFmpeg)
    //  - Pixel format (tells FFmpeg which hwaccel to use)
    //  - Array of AVCodecID (potential codecs that can be accelerated by the hwaccel)
    //  Note: an empty name means the video isn't accelerated
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
*/
/*
const HardwareAccel
setupHardwareEncoding(AVCodecContext* codecCtx, AVCodec** codec)
{
    //,"omx","qsv","vaapi","videotoolbox"
    std::map<enum AVCodecID,std::vector<std::string>>map_hw_encoders;
    map_hw_encoders[AV_CODEC_ID_H264] = {"h264_nvenc", "h264_qsv", "h264_vaapi", "h264_videotoolbox"};
    map_hw_encoders[AV_CODEC_ID_HEVC] = {"hevc_nvenc","hevc_qsv","hevc_nvenc","hevc_vaapi","hevc_videotoolbox"};
    map_hw_encoders[AV_CODEC_ID_MJPEG] = {"mjpeg_qsv","mjpeg_vaapi"};
    map_hw_encoders[AV_CODEC_ID_MPEG2VIDEO] = {"mpeg2_qsv","mpeg2_vaapi"};
    map_hw_encoders[AV_CODEC_ID_VP8] = {"vp8_vaapi"};
    map_hw_encoders[AV_CODEC_ID_VP9] = {"vp9_vaapi"};

    if (codecCtx->codec_id)
    RING_WARN("input codec id = %s\n", avcodec_get_name(codecCtx->codec_id));
    auto it = map_hw_encoders.find(codecCtx->codec_id);
    if (it != map_hw_encoders.end()) {
        for (unsigned int i=0 ; i < it->second.size() ; i++){
            RING_WARN("codec in list = %s", it->second[i].c_str());
            if ((*codec = avcodec_find_encoder_by_name((it->second[i]).c_str()))){
                codecCtx->codec=*codec;
                HardwareAccel accel;
                accel.name = it->second[i].substr(it->second[i].find("_") + 1);
                //suppose pixel format is always set to be vaapi
                //accel.format = AV_PIX_FMT_VAAPI;
                //accel.supportedCodecs = ;
                if (initDevice(accel, codecCtx) >= 0) {
                    //add init hw_frame_ctx
                    initHwFrameCtx(codecCtx);
                    codecCtx->thread_safe_callbacks = 1;
                    RING_WARN("Found a hw encoder for %s, %s\n",avcodec_get_name(codecCtx->codec_id), it->second[i].c_str());
                    return accel;
                }
            }
        }
    }

    RING_WARN("Not using hardware accelerated encoding");
    return {};
}
*/

}} // namespace ring::video
