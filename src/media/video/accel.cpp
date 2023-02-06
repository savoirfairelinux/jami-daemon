/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "media_buffer.h"
#include "string_utils.h"
#include "fileutils.h"
#include "logger.h"
#include "accel.h"

namespace jami {
namespace video {

struct HardwareAPI
{
    std::string name;
    AVHWDeviceType hwType;
    AVPixelFormat format;
    AVPixelFormat swFormat;
    std::vector<AVCodecID> supportedCodecs;
    std::list<std::pair<std::string, DeviceState>> possible_devices;
    bool dynBitrate;
};


static std::list<HardwareAPI> apiListDec = {
    {"nvdec",
     AV_HWDEVICE_TYPE_CUDA,
     AV_PIX_FMT_CUDA,
     AV_PIX_FMT_NV12,
     {AV_CODEC_ID_H264, AV_CODEC_ID_HEVC, AV_CODEC_ID_VP8, AV_CODEC_ID_MJPEG},
     {{"default", DeviceState::NOT_TESTED}, {"1", DeviceState::NOT_TESTED}, {"2", DeviceState::NOT_TESTED}},
     false},
    {"vaapi",
     AV_HWDEVICE_TYPE_VAAPI,
     AV_PIX_FMT_VAAPI,
     AV_PIX_FMT_NV12,
     {AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4, AV_CODEC_ID_VP8},
     {{"default", DeviceState::NOT_TESTED}, {"/dev/dri/renderD128", DeviceState::NOT_TESTED},
      {"/dev/dri/renderD129", DeviceState::NOT_TESTED}, {":0", DeviceState::NOT_TESTED}},
     false},
    {"vdpau",
     AV_HWDEVICE_TYPE_VDPAU,
     AV_PIX_FMT_VDPAU,
     AV_PIX_FMT_NV12,
     {AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4},
     {{"default", DeviceState::NOT_TESTED}},
     false},
    {"videotoolbox",
     AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
     AV_PIX_FMT_VIDEOTOOLBOX,
     AV_PIX_FMT_NV12,
     {AV_CODEC_ID_H264, AV_CODEC_ID_HEVC, AV_CODEC_ID_MPEG4},
     {{"default", DeviceState::NOT_TESTED}},
     false},
    {"qsv",
     AV_HWDEVICE_TYPE_QSV,
     AV_PIX_FMT_QSV,
     AV_PIX_FMT_NV12,
     {AV_CODEC_ID_H264, AV_CODEC_ID_HEVC, AV_CODEC_ID_MJPEG, AV_CODEC_ID_VP8, AV_CODEC_ID_VP9},
     {{"default", DeviceState::NOT_TESTED}},
     false},
};

static std::list<HardwareAPI> apiListEnc = {
    {"nvenc",
     AV_HWDEVICE_TYPE_CUDA,
     AV_PIX_FMT_CUDA,
     AV_PIX_FMT_NV12,
     {AV_CODEC_ID_H264, AV_CODEC_ID_HEVC},
     {{"default", DeviceState::NOT_TESTED}, {"1", DeviceState::NOT_TESTED}, {"2", DeviceState::NOT_TESTED}},
     true},
    {"vaapi",
     AV_HWDEVICE_TYPE_VAAPI,
     AV_PIX_FMT_VAAPI,
     AV_PIX_FMT_NV12,
     {AV_CODEC_ID_H264, AV_CODEC_ID_HEVC, AV_CODEC_ID_VP8},
     {{"default", DeviceState::NOT_TESTED}, {"/dev/dri/renderD128", DeviceState::NOT_TESTED},
      {"/dev/dri/renderD129", DeviceState::NOT_TESTED},
      {":0", DeviceState::NOT_TESTED}},
     false},
    {"videotoolbox",
     AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
     AV_PIX_FMT_VIDEOTOOLBOX,
     AV_PIX_FMT_NV12,
     {AV_CODEC_ID_H264, AV_CODEC_ID_HEVC},
     {{"default", DeviceState::NOT_TESTED}},
     false},
    // Disable temporarily QSVENC
    // {"qsv",
    // AV_HWDEVICE_TYPE_QSV,
    //  AV_PIX_FMT_QSV,
    //  AV_PIX_FMT_NV12,
    // {AV_CODEC_ID_H264, AV_CODEC_ID_HEVC, AV_CODEC_ID_MJPEG, AV_CODEC_ID_VP8},
    // {{"default", DeviceState::NOT_TESTED}},
    // false},
};

HardwareAccel::HardwareAccel(AVCodecID id,
                             const std::string& name,
                             AVHWDeviceType hwType,
                             AVPixelFormat format,
                             AVPixelFormat swFormat,
                             CodecType type,
                             bool dynBitrate)
    : id_(id)
    , name_(name)
    , hwType_(hwType)
    , format_(format)
    , swFormat_(swFormat)
    , type_(type)
    , dynBitrate_(dynBitrate)
{}

HardwareAccel::~HardwareAccel()
{
    if (deviceCtx_)
        av_buffer_unref(&deviceCtx_);
    if (framesCtx_)
        av_buffer_unref(&framesCtx_);
}

static AVPixelFormat
getFormatCb(AVCodecContext* codecCtx, const AVPixelFormat* formats)
{
    auto accel = static_cast<HardwareAccel*>(codecCtx->opaque);

    for (int i = 0; formats[i] != AV_PIX_FMT_NONE; ++i) {
        if (accel && formats[i] == accel->getFormat()) {
            // found hardware format for codec with api
            JAMI_DBG() << "Found compatible hardware format for "
                       << avcodec_get_name(static_cast<AVCodecID>(accel->getCodecId()))
                       << " decoder with " << accel->getName();
            // hardware tends to under-report supported levels
            codecCtx->hwaccel_flags |= AV_HWACCEL_FLAG_IGNORE_LEVEL;
            return formats[i];
        }
    }
    return AV_PIX_FMT_NONE;
}

int
HardwareAccel::init_device(const char* name, const char* device, int flags)
{
    const AVHWDeviceContext* dev = nullptr;

    // Create device ctx
    int err;
    err = av_hwdevice_ctx_create(&deviceCtx_, hwType_, device, NULL, flags);
    if (err < 0) {
        JAMI_DBG("Failed to create %s device: %d.\n", name, err);
        return 1;
    }

    // Verify that the device create correspond to api
    dev = (AVHWDeviceContext*) deviceCtx_->data;
    if (dev->type != hwType_) {
        JAMI_DBG("Device created as type %d has type %d.", hwType_, dev->type);
        av_buffer_unref(&deviceCtx_);
        return -1;
    }
    JAMI_DBG("Device type %s successfully created.", name);

    return 0;
}

int
HardwareAccel::init_device_type(std::string& dev)
{
    AVHWDeviceType check;
    const char* name;
    int err;

    name = av_hwdevice_get_type_name(hwType_);
    if (!name) {
        JAMI_DBG("No name available for device type %d.", hwType_);
        return -1;
    }

    check = av_hwdevice_find_type_by_name(name);
    if (check != hwType_) {
        JAMI_DBG("Type %d maps to name %s maps to type %d.", hwType_, name, check);
        return -1;
    }

    JAMI_WARN("-- Starting %s init for %s with default device.",
              (type_ == CODEC_ENCODER) ? "encoding" : "decoding",
              name);
    if (possible_devices_->front().second != DeviceState::NOT_USABLE) {
        if (name_ == "qsv")
            err = init_device(name, "auto", 0);
        else
            err = init_device(name, nullptr, 0);
        if (err == 0) {
            JAMI_DBG("-- Init passed for %s with default device.", name);
            possible_devices_->front().second = DeviceState::USABLE;
            dev = "default";
            return 0;
        } else {
            possible_devices_->front().second = DeviceState::NOT_USABLE;
            JAMI_DBG("-- Init failed for %s with default device.", name);
        }
    }

    for (auto& device : *possible_devices_) {
        if (device.second == DeviceState::NOT_USABLE)
            continue;
        JAMI_WARN("-- Init %s for %s with device %s.",
                  (type_ == CODEC_ENCODER) ? "encoding" : "decoding",
                  name,
                  device.first.c_str());
        err = init_device(name, device.first.c_str(), 0);
        if (err == 0) {
            JAMI_DBG("-- Init passed for %s with device %s.", name, device.first.c_str());
            device.second = DeviceState::USABLE;
            dev = device.first;
            return 0;
        } else {
            device.second = DeviceState::NOT_USABLE;
            JAMI_DBG("-- Init failed for %s with device %s.", name, device.first.c_str());
        }
    }
    return -1;
}

std::string
HardwareAccel::getCodecName() const
{
    if (type_ == CODEC_DECODER) {
        return avcodec_get_name(id_);
    } else if (type_ == CODEC_ENCODER) {
        return fmt::format("{}_{}", avcodec_get_name(id_), name_);
    }
    return {};
}

std::unique_ptr<VideoFrame>
HardwareAccel::transfer(const VideoFrame& frame)
{
    int ret = 0;
    if (type_ == CODEC_DECODER) {
        auto input = frame.pointer();
        if (input->format != format_) {
            JAMI_ERR() << "Frame format mismatch: expected " << av_get_pix_fmt_name(format_)
                       << ", got "
                       << av_get_pix_fmt_name(static_cast<AVPixelFormat>(input->format));
            return nullptr;
        }

        return transferToMainMemory(frame, swFormat_);
    } else if (type_ == CODEC_ENCODER) {
        auto input = frame.pointer();
        if (input->format != swFormat_) {
            JAMI_ERR() << "Frame format mismatch: expected " << av_get_pix_fmt_name(swFormat_)
                       << ", got "
                       << av_get_pix_fmt_name(static_cast<AVPixelFormat>(input->format));
            return nullptr;
        }

        auto framePtr = std::make_unique<VideoFrame>();
        auto hwFrame = framePtr->pointer();

        if ((ret = av_hwframe_get_buffer(framesCtx_, hwFrame, 0)) < 0) {
            JAMI_ERR() << "Failed to allocate hardware buffer: "
                       << libav_utils::getError(ret).c_str();
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
HardwareAccel::initFrame()
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
    ctx->width = width_;
    ctx->height = height_;
    ctx->initial_pool_size = 20; // TODO try other values

    if ((ret = av_hwframe_ctx_init(framesCtx_)) < 0) {
        JAMI_ERR("Failed to initialize hardware frame context: %s (%d)",
                 libav_utils::getError(ret).c_str(),
                 ret);
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
    if (not input)
        throw std::runtime_error("Cannot transfer null frame");

    auto desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(input->format));
    if (!desc) {
        throw std::runtime_error("Cannot transfer frame with invalid format");
    }

    auto out = std::make_unique<VideoFrame>();
    if (not(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        out->copyFrom(frame);
        return out;
    }

    auto output = out->pointer();
    output->format = desiredFormat;

    int ret = av_hwframe_transfer_data(output, input, 0);
    if (ret < 0) {
        throw std::runtime_error("Cannot transfer the frame from GPU");
    }

    output->pts = input->pts;
    if (AVFrameSideData* side_data = av_frame_get_side_data(input, AV_FRAME_DATA_DISPLAYMATRIX))
        av_frame_new_side_data_from_buf(output,
                                        AV_FRAME_DATA_DISPLAYMATRIX,
                                        av_buffer_ref(side_data->buf));
    return out;
}

int
HardwareAccel::initAPI(bool linkable, AVBufferRef* framesCtx)
{
    const auto& codecName = getCodecName();
    std::string device;
    auto ret = init_device_type(device);
    if (ret == 0) {
        bool link = false;
        if (linkable && framesCtx)
            link = linkHardware(framesCtx);
        // we don't need frame context for videotoolbox
        if (hwType_ == AV_HWDEVICE_TYPE_VIDEOTOOLBOX || link || initFrame()) {
            return 0;
        }
    }
    return -1;
}

std::list<HardwareAccel>
HardwareAccel::getCompatibleAccel(AVCodecID id, int width, int height, CodecType type)
{
    std::list<HardwareAccel> l;
    const auto& list = (type == CODEC_ENCODER) ? &apiListEnc : &apiListDec;
    for (auto& api : *list) {
        const auto& it = std::find(api.supportedCodecs.begin(), api.supportedCodecs.end(), id);
        if (it != api.supportedCodecs.end()) {
            auto hwtype = AV_HWDEVICE_TYPE_NONE;
            while ((hwtype = av_hwdevice_iterate_types(hwtype)) != AV_HWDEVICE_TYPE_NONE) {
                if (hwtype == api.hwType) {
                    auto accel = HardwareAccel(id,
                                               api.name,
                                               api.hwType,
                                               api.format,
                                               api.swFormat,
                                               type,
                                               api.dynBitrate);
                    accel.height_ = height;
                    accel.width_ = width;
                    accel.possible_devices_ = &api.possible_devices;
                    l.emplace_back(std::move(accel));
                }
            }
        }
    }
    return l;
}

} // namespace video
} // namespace jami
