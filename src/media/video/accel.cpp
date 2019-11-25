/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
 *  Author: Pierre Lespagnol <pierre.lespagnol@savoirfairelinux.com>
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

#include <algorithm>

#include "media_buffer.h"
#include "string_utils.h"
#include "fileutils.h"
#include "logger.h"
#include "accel.h"
#include "config.h"

namespace jami { namespace video {

static const HardwareAPI apiListDec[] = {
    { "nvdec", AV_HWDEVICE_TYPE_CUDA, AV_PIX_FMT_CUDA, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_H265, AV_CODEC_ID_VP8, AV_CODEC_ID_MJPEG }, { "0", "1", "2" } },
    { "vaapi", AV_HWDEVICE_TYPE_VAAPI, AV_PIX_FMT_VAAPI, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_VP8, AV_CODEC_ID_MJPEG }, { "/dev/dri/renderD128", "/dev/dri/renderD129", ":0" } },
    { "vdpau", AV_HWDEVICE_TYPE_VDPAU, AV_PIX_FMT_VDPAU, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4 }, { } },
    { "videotoolbox", AV_HWDEVICE_TYPE_VIDEOTOOLBOX, AV_PIX_FMT_VIDEOTOOLBOX, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4 }, { } },
    { "qsv", AV_HWDEVICE_TYPE_QSV, AV_PIX_FMT_QSV, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_H265, AV_CODEC_ID_MJPEG, AV_CODEC_ID_VP8, AV_CODEC_ID_VP9 }, { } },
};

static const HardwareAPI apiListEnc[] = {
    { "nvenc", AV_HWDEVICE_TYPE_CUDA, AV_PIX_FMT_CUDA, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_H265 }, { "0", "1", "2" } },
    { "vaapi", AV_HWDEVICE_TYPE_VAAPI, AV_PIX_FMT_VAAPI, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_MJPEG, AV_CODEC_ID_VP8 }, { "/dev/dri/renderD128", "/dev/dri/renderD129", ":0" } },
    { "videotoolbox", AV_HWDEVICE_TYPE_VIDEOTOOLBOX, AV_PIX_FMT_VIDEOTOOLBOX, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264 }, { } },
    { "qsv", AV_HWDEVICE_TYPE_QSV, AV_PIX_FMT_QSV, AV_PIX_FMT_NV12, { AV_CODEC_ID_H264, AV_CODEC_ID_H265, AV_CODEC_ID_MJPEG, AV_CODEC_ID_VP8 }, { } },
};

int
HardwareAccel::test_device(const HardwareAPI& api, const char* name,
                        const char* device, int flags)
{
    AVBufferRef *ref;
    AVHWDeviceContext *dev;
    int err;

    err = av_hwdevice_ctx_create(&ref, api.hwType, device, NULL, flags);
    if (err < 0) {
        JAMI_DBG("Failed to create %s device: %d.\n", name, err);
        return 1;
    }

    dev = (AVHWDeviceContext*)ref->data;
    if (dev->type != api.hwType) {
        JAMI_DBG("Device created as type %d has type %d.",
                api.hwType, dev->type);
        av_buffer_unref(&ref);
        return -1;
    }

    JAMI_DBG("Device type %s successfully created.", name);
    av_buffer_unref(&ref);

    return err;
}

std::string
HardwareAccel::test_device_type(const HardwareAPI& api)
{
    AVHWDeviceType check;
    const char *name;
    int err;

    name = av_hwdevice_get_type_name(api.hwType);
    if (!name) {
        JAMI_DBG("No name available for device type %d.", api.hwType);
        return "";
    }

    check = av_hwdevice_find_type_by_name(name);
    if (check != api.hwType) {
        JAMI_DBG("Type %d maps to name %s maps to type %d.",
               api.hwType, name, check);
        return "";
    }

    err = test_device(api, name, NULL, 0);
    if (err == 0) {
        JAMI_DBG("O Test passed for %s with default options.", name);
        return "default";
    } else {
        JAMI_DBG("X Test failed for %s with default options.", name);
    }

    for (unsigned j = 0; j < api.possible_devices.size(); j++) {
        err = test_device(api, name, api.possible_devices[j].c_str(), 0);
        if (err == 0) {
            JAMI_DBG("O Test passed for %s with device %s.",
                    name, api.possible_devices[j].c_str());
            return api.possible_devices[j];
        }
        else {
            JAMI_DBG("X Test failed for %s with device %s.",
                    name, api.possible_devices[j].c_str());
        }
    }

    return "";
}

static AVPixelFormat
getFormatCb(AVCodecContext* codecCtx, const AVPixelFormat* formats)
{
    auto accel = static_cast<HardwareAccel*>(codecCtx->opaque);

    AVPixelFormat fallback = AV_PIX_FMT_NONE;
    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; ++i) {
        fallback = formats[i];
        if (accel && formats[i] == accel->getFormat()) {
            // found hardware format for codec with api
            JAMI_DBG() << "Found compatible hardware format for "
                << avcodec_get_name(static_cast<AVCodecID>(accel->getCodecId()))
                << " decoder with " << accel->getName();
            return formats[i];
        }
    }

    JAMI_WARN() << "Not using hardware decoding";
    return fallback;
}

HardwareAccel::HardwareAccel(AVCodecID id, const std::string& name, AVHWDeviceType hwType, AVPixelFormat format, AVPixelFormat swFormat, CodecType type)
    : id_(id)
    , name_(name)
    , hwType_(hwType)
    , format_(format)
    , swFormat_(swFormat)
    , type_(type)
{}

HardwareAccel::~HardwareAccel()
{
    if (deviceCtx_)
        av_buffer_unref(&deviceCtx_);
    if (framesCtx_)
        av_buffer_unref(&framesCtx_);
}

std::string
HardwareAccel::getCodecName() const
{
    if (type_ == CODEC_DECODER) {
        return avcodec_get_name(id_);
    } else if (type_ == CODEC_ENCODER) {
        std::stringstream ss;
        ss << avcodec_get_name(id_) << '_' << name_;
        return ss.str();
    }
    return "";
}

std::unique_ptr<VideoFrame>
HardwareAccel::transfer(const VideoFrame& frame)
{
    int ret = 0;
    if (type_ == CODEC_DECODER) {
        auto input = frame.pointer();
        if (input->format != format_) {
            JAMI_ERR() << "Frame format mismatch: expected "
                << av_get_pix_fmt_name(format_) << ", got "
                << av_get_pix_fmt_name(static_cast<AVPixelFormat>(input->format));
            return nullptr;
        }

        return transferToMainMemory(frame, swFormat_);
    } else if (type_ == CODEC_ENCODER) {
        auto input = frame.pointer();
        if (input->format != swFormat_) {
            JAMI_ERR() << "Frame format mismatch: expected "
                << av_get_pix_fmt_name(swFormat_) << ", got "
                << av_get_pix_fmt_name(static_cast<AVPixelFormat>(input->format));
            return nullptr;
        }

        auto framePtr = std::make_unique<VideoFrame>();
        auto hwFrame = framePtr->pointer();

        if ((ret = av_hwframe_get_buffer(framesCtx_, hwFrame, 0)) < 0) {
            JAMI_ERR() << "Failed to allocate hardware buffer: " << libav_utils::getError(ret).c_str();
            return nullptr;
        }

        if (!hwFrame->hw_frames_ctx) {
            JAMI_ERR() << "Failed to allocate hardware buffer: Cannot allocate memory";
            return nullptr;
        }

        if ((ret = av_hwframe_transfer_data(hwFrame, input, 0)) < 0) {
            JAMI_ERR() << "Failed to push frame to GPU: " << libav_utils::getError(ret).c_str();
            return nullptr;
        }

        hwFrame->pts = input->pts; // transfer does not copy timestamp
        return framePtr;
    } else {
        JAMI_ERR() << "Invalid hardware accelerator";
        return nullptr;
    }
}

void
HardwareAccel::setDetails(AVCodecContext* codecCtx)
{
    if (type_ == CODEC_DECODER) {
        codecCtx->hw_device_ctx = av_buffer_ref(deviceCtx_);
        codecCtx->get_format = getFormatCb;
        codecCtx->thread_safe_callbacks = 1;
    } else if (type_ == CODEC_ENCODER) {
        if (framesCtx_)
            // encoder doesn't need a device context, only a frame context
            codecCtx->hw_frames_ctx = av_buffer_ref(framesCtx_);
    }
}

bool
HardwareAccel::initDevice(std::string device)
{
    int ret;
    if (device == "default")
        ret = av_hwdevice_ctx_create(&deviceCtx_, hwType_, nullptr, nullptr, 0);
    else
        ret = av_hwdevice_ctx_create(&deviceCtx_, hwType_, device.c_str(), nullptr, 0);

    if (ret < 0)
        JAMI_ERR("Creating hardware device context failed: %s (%d)", libav_utils::getError(ret).c_str(), ret);
    return ret >= 0;
}

bool
HardwareAccel::initFrame(int width, int height)
{
    int ret = 0;
    if (!deviceCtx_) {
        JAMI_ERR() << "Cannot initialize hardware frames without a valid hardware device";
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

    if ((ret = av_hwframe_ctx_init(framesCtx_)) < 0) {
        JAMI_ERR("Failed to initialize hardware frame context: %s (%d)", libav_utils::getError(ret).c_str(), ret);
        av_buffer_unref(&framesCtx_);
    }

    return ret >= 0;
}

bool
HardwareAccel::linkHardware(AVBufferRef* framesCtx)
{
    if (framesCtx) {
        // Force sw_format to match swFormat_. Frame is never transferred to main
        // memory when hardware is linked, so the sw_format doesn't matter.
        auto hw = reinterpret_cast<AVHWFramesContext*>(framesCtx->data);
        hw->sw_format = swFormat_;

        if (framesCtx_)
            av_buffer_unref(&framesCtx_);
        framesCtx_ = av_buffer_ref(framesCtx);
        if ((linked_ = (framesCtx_ != nullptr))) {
            JAMI_DBG() << "Hardware transcoding pipeline successfully set up for"
                << " encoder '" << getCodecName() << "'";
        }
        return linked_;
    } else {
        return false;
    }
}

std::unique_ptr<VideoFrame>
HardwareAccel::transferToMainMemory(const VideoFrame& frame, AVPixelFormat desiredFormat)
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
    if (AVFrameSideData* side_data = av_frame_get_side_data(input, AV_FRAME_DATA_DISPLAYMATRIX))
        av_frame_new_side_data_from_buf(output, AV_FRAME_DATA_DISPLAYMATRIX, av_buffer_ref(side_data->buf));
    return out;
}

std::unique_ptr<HardwareAccel>
HardwareAccel::setupDecoder(AVCodecID id, int width, int height)
{
    for (const auto& api : apiListDec) {
        if (std::find(api.supportedCodecs.begin(), api.supportedCodecs.end(), id) != api.supportedCodecs.end()) {
            auto accel = std::make_unique<HardwareAccel>(id, api.name, api.hwType, api.format, api.swFormat, CODEC_DECODER);
            std::string device = test_device_type(api);
            if(device != "" && accel->initDevice(device)) {
                // we don't need frame context for videotoolbox
                if (api.format == AV_PIX_FMT_VIDEOTOOLBOX || accel->initFrame(width, height)) {
                    JAMI_DBG() << "Using hardware decoder " << accel->getCodecName() << " with " << api.name << ", device: " << device;
                    return accel;
                }
            }
        }
    }
    JAMI_WARN() << "Not using hardware decoding";
    return nullptr;
}

std::unique_ptr<HardwareAccel>
HardwareAccel::setupEncoder(AVCodecID id, int width, int height, bool linkable, AVBufferRef* framesCtx)
{
    for (auto api : apiListEnc) {
        const auto& it = std::find(api.supportedCodecs.begin(), api.supportedCodecs.end(), id);
        if (it != api.supportedCodecs.end()) {
            auto accel = std::make_unique<HardwareAccel>(id, api.name, api.hwType, api.format, api.swFormat, CODEC_ENCODER);
            const auto& codecName = accel->getCodecName();
            if (avcodec_find_encoder_by_name(codecName.c_str())) {
                std::string device = test_device_type(api);
                if(device != "" && accel->initDevice(device)) {
                    bool link = false;
                    if (linkable)
                        link = accel->linkHardware(framesCtx);
                    // we don't need frame context for videotoolbox
                    if (api.format == AV_PIX_FMT_VIDEOTOOLBOX || accel->linkHardware(framesCtx) ||
                       link || accel->initFrame(width, height)) {
                        JAMI_DBG() << "Using hardware encoder " << codecName << " with " << api.name << ", device: " << device;
                        return accel;
                    }
                }
            }
        }
    }
    JAMI_WARN() << "Not using hardware encoding";
    return nullptr;
}

}} // namespace jami::video
