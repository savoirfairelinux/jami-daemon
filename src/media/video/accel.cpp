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
	if (accel.name != "omx"){
		RING_WARN("hwType = %d", hwType);
		RING_WARN("hardwareDeviceCtx = %p", hardwareDeviceCtx);
		// default device (nullptr) works for most cases
		if ((ret = av_hwdevice_ctx_create(&hardwareDeviceCtx, hwType, nullptr, nullptr, 0)) >= 0) {
			codecCtx->hw_device_ctx = hardwareDeviceCtx;
			RING_DBG("Using '%s' hardware acceleration", accel.name.c_str());
		}
		else{
			RING_ERR()<<"av_hwdevice_ctx_create error: "<<libav_utils::getError(ret);
		}
	}
    return ret;
}

int
initHwFrameCtx(HardwareAccel accel, AVCodecContext *ctx){
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = NULL;
    int err = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(ctx->hw_device_ctx))) {
        fprintf(stderr, "Failed to create VAAPI frame context.");
        return -1;
    }

    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = accel.format;
    if (frames_ctx->format == AV_PIX_FMT_VAAPI)
        frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width     = ctx->width;
    frames_ctx->height    = ctx->height;
    frames_ctx->initial_pool_size = 20;
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        fprintf(stderr, "Failed to initialize VAAPI frame context."
                "Error code: %s",libav_utils::getError(err).c_str());
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
		{ "omx", AV_PIX_FMT_YUV420P, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263, AV_CODEC_ID_VP8, AV_CODEC_ID_MJPEG } },
        { "vaapi", AV_PIX_FMT_VAAPI, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263, AV_CODEC_ID_VP8, AV_CODEC_ID_MJPEG } },
        { "vdpau", AV_PIX_FMT_VDPAU, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263 } },
        { "videotoolbox", AV_PIX_FMT_VIDEOTOOLBOX, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_H263 } },
    };

	if (codecCtx->codec_id)
        RING_WARN("input decoding codec id = %s", avcodec_get_name(codecCtx->codec_id));

    for (auto accel : accels) {
        if (std::find(accel.supportedCodecs.begin(), accel.supportedCodecs.end(),
                static_cast<AVCodecID>(codecCtx->codec_id)) != accel.supportedCodecs.end()) {
			RING_WARN("find = %s", avcodec_get_name(codecCtx->codec_id));
            if (accel.name == "omx"){
				RING_WARN("Found a hw decoder for %s.", accel.name.c_str());
				return accel;
			}
            else if (initDevice(accel, codecCtx) >= 0) {
                codecCtx->get_format = getFormatCb;
                codecCtx->thread_safe_callbacks = 1;
                RING_WARN("Found a hw decoder for %s.", accel.name);
                return accel;
            }
        }
    }
	std::string av_type;
	if (codecCtx->codec_type == AVMEDIA_TYPE_UNKNOWN)
		av_type = " for unknown type";
	else if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
		av_type = " for video";
	else if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO)
		av_type = " for audio";
    RING_WARN("Not using hardware accelerated decoding%s",av_type.c_str());
    return {};
}

const HardwareAccel
setupHardwareEncoding(AVCodecContext** codecCtx, AVCodec** codec)
{
    //select codec API based on the provided codec ID
    std::map<enum AVCodecID,std::vector<std::string>>map_hw_encoders;
    map_hw_encoders[AV_CODEC_ID_H264] = { "h264_omx", "h264_vaapi", "h264_videotoolbox"};
    map_hw_encoders[AV_CODEC_ID_HEVC] = {"hevc_vaapi","hevc_videotoolbox"};
    map_hw_encoders[AV_CODEC_ID_MJPEG] = {"mjpeg_vaapi"};
    map_hw_encoders[AV_CODEC_ID_MPEG2VIDEO] = {"mpeg2_vaapi"};
    map_hw_encoders[AV_CODEC_ID_VP8] = {"vp8_vaapi"};
    map_hw_encoders[AV_CODEC_ID_VP9] = {"vp9_vaapi"};
    //select pixel format based on the given API
    std::map<std::string,enum AVPixelFormat>map_pix_fmt;
    map_pix_fmt["vaapi"]={AV_PIX_FMT_VAAPI};
    map_pix_fmt["videotoolbox"]={AV_PIX_FMT_VIDEOTOOLBOX};
    map_pix_fmt["omx"]={AV_PIX_FMT_YUV420P};
    if ((*codecCtx)->codec_id)
        RING_WARN("input encoding codec id = %s", avcodec_get_name((*codecCtx)->codec_id));
    auto it = map_hw_encoders.find((*codecCtx)->codec_id);
    if (it != map_hw_encoders.end()) {
        for (unsigned int i=0 ; i < it->second.size() ; i++){
            RING_WARN("codec in list = %s", it->second[i].c_str());
            if ((*codec = avcodec_find_encoder_by_name((it->second[i]).c_str()))){
                //reprepare codeccontext for new codec
                AVCodecContext* encoderCtx = avcodec_alloc_context3(*codec);
                auto encoderName = encoderCtx->av_class->item_name ?
                    encoderCtx->av_class->item_name(encoderCtx) : nullptr;
                if (encoderName == nullptr)
                    encoderName = "encoder?";
                encoderCtx->width = (*codecCtx)->width;
                encoderCtx->height = (*codecCtx)->height;
                // satisfy ffmpeg: denominator must be 16bit or less value
                // time base = 1/FPS
                encoderCtx->time_base = (*codecCtx)->time_base;
                encoderCtx->framerate = (*codecCtx)->framerate;
                // emit one intra frame every gop_size frames
                encoderCtx->max_b_frames = 0;
                *codecCtx = encoderCtx;
                /////////////////////////////////////////////////////
                HardwareAccel accel;
                accel.name = it->second[i].substr(it->second[i].find("_") + 1);
                if (accel.name == "omx")
					return {};
                auto it_pf = map_pix_fmt.find(accel.name);
                accel.format = it_pf->second;
                encoderCtx->pix_fmt = it_pf->second;
                if (initDevice(accel, *codecCtx) >= 0) {
                    //add init hw_frame_ctx
                    initHwFrameCtx(accel, *codecCtx);
                    RING_WARN("in setuphwencoding hw_frames_ctx = %p",(*codecCtx)->hw_frames_ctx);
                    (*codecCtx)->thread_safe_callbacks = 1;
                    RING_WARN("Found a hw encoder for %s, %s",avcodec_get_name((*codecCtx)->codec_id), it->second[i].c_str());
                    return accel;
                }
            }
        }
    }
	std::string av_type;
	if ((*codecCtx)->codec_type == AVMEDIA_TYPE_UNKNOWN)
		av_type = " for unknown type";
	else if ((*codecCtx)->codec_type == AVMEDIA_TYPE_VIDEO)
		av_type = " for video";
	else if ((*codecCtx)->codec_type == AVMEDIA_TYPE_AUDIO)
		av_type = " for audio";
    RING_WARN("Not using hardware accelerated encoding%s",av_type.c_str());
    return {};
}

}} // namespace ring::video
