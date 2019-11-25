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
#include <thread> // hardware_concurrency

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
HardwareAccel::set_hwframe_ctx(AVCodecContext* avctx, AVBufferRef* hw_device_ctx)
{
    AVBufferRef* hw_frames_ref;
    AVHWFramesContext* frames_ctx = NULL;
    int err = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx))) {
        JAMI_DBG("Failed to create hardware frame context.");
        return -1;
    }
    // JAMI_WARN("OK HW frame alloc");
    frames_ctx = (AVHWFramesContext*)(hw_frames_ref->data);
    frames_ctx->format    = format_;
    frames_ctx->sw_format = swFormat_;
    frames_ctx->width     = width_;
    frames_ctx->height    = height_;
    frames_ctx->initial_pool_size = 20;
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        fprintf(stderr, "Failed to initialize hardware frame context."
                "Error code: %s\n",libav_utils::getError(err).c_str());
        av_buffer_unref(&hw_frames_ref);
        return err;
    }
    // JAMI_WARN("OK HW frame init");
    avctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!avctx->hw_frames_ctx) {
        JAMI_WARN("Failed to init HW frame.");
        return -1;
    }

    av_buffer_unref(&hw_frames_ref);
    return err;
}

int 
HardwareAccel::init_codec_ctx(AVCodecContext** actx, AVBufferRef* hw_device_ctx)
{
    AVCodecContext* ctx;
    // Find codec
    auto name = getCodecName().c_str();
    AVCodec* codec = nullptr;
    if (type_ == CODEC_DECODER)
        codec = avcodec_find_decoder_by_name(name);
    else if (type_ == CODEC_ENCODER)
        codec = avcodec_find_encoder_by_name(name);
    
    if (!codec) {
        JAMI_DBG("Could not find codec: %s", name);
        return -1;
    }
    // JAMI_WARN("OK codec found: %s", name);

    // Alloc encoder ctx
    if (!(ctx = avcodec_alloc_context3(codec))) {
        JAMI_DBG("Failed to alloc codec");
        return -1;
    }
    // JAMI_WARN("OK codec alloc");

    // Init codec ctx
    if (type_  == CODEC_ENCODER) {
        ctx->thread_count = std::min(std::thread::hardware_concurrency(), 16u);
        ctx->pix_fmt = format_;
    }
    else {
        ctx->pix_fmt = fmtDec_;
    }
    ctx->width = width_;
    ctx->height = height_;
    av_reduce(&(ctx->framerate.num), &(ctx->framerate.den),
                30, 1, (1U << 16) - 1);
    ctx->time_base = av_inv_q(ctx->framerate);
    // emit one intra frame every gop_size frames
    ctx->max_b_frames = 0;
    ctx->level = 0x0d;

    // Open decoder
    int err;
    if (type_ == CODEC_DECODER) {
        if ((err = avcodec_open2(ctx, codec, NULL)) < 0) {
            JAMI_DBG("Cannot open hardware decoder codec. Error code: %s\n",
                        libav_utils::getError(err).c_str());
            return -1;
        }
        // JAMI_WARN("OK open hardware decoder");
        return 0;       // Ok for decoder -> stop the test
    }

    /* set hw_frames_ctx for encoder's AVCodecContext */
    if (set_hwframe_ctx(ctx, hw_device_ctx) < 0) {
        JAMI_DBG("Failed to set hwframe context.\n");
        return -1;
    }
    // JAMI_WARN("OK set hw frame context");

    // Open encoder
    if ((err = avcodec_open2(ctx, codec, NULL)) < 0) {
        JAMI_DBG("Cannot open video encoder codec. Error code: %s\n", libav_utils::getError(err).c_str());
        return -1;
    }
    // JAMI_WARN("OK open encoder");
    *actx = ctx;
    return 1;       // Ok for encoder -> continue the test
}

void 
HardwareAccel::close_codec_ctx(AVCodecContext* avctx, AVBufferRef* hw_device_ctx, AVFrame* sw_frame, AVFrame* hw_frame)
{
    if (sw_frame)
        av_frame_free(&sw_frame);
    if (hw_frame)
        av_frame_free(&hw_frame);

    avcodec_free_context(&avctx);
    av_buffer_unref(&hw_device_ctx);
}

int 
HardwareAccel::test_encode_black_frame(AVCodecContext* avctx, AVFrame* sw_frame, AVFrame* hw_frame)
{
    // Alloc frame hw and sw
    if (!(hw_frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
        JAMI_DBG("Can not alloc frame\n");
        return -1;
    }
    // JAMI_WARN("OK alloc frame");

    // Init sw frame
    sw_frame->format = swFormat_;
    sw_frame->width  = width_;
    sw_frame->height = height_;
    int err;
    if ((err = av_frame_get_buffer(sw_frame, 32)) < 0){
        JAMI_DBG("Error code: %s.\n", libav_utils::getError(err).c_str());
        return -1;
    }
    libav_utils::fillWithBlack(sw_frame);
    // JAMI_WARN("OK Frame filled in black");

    // Verify hardware frame context
    if ((err = av_hwframe_get_buffer(avctx->hw_frames_ctx, hw_frame, 0)) < 0) {
        JAMI_DBG("Error code: %s.\n", libav_utils::getError(err).c_str());
        return -1;
    }

    if (!hw_frame->hw_frames_ctx) {
        err = AVERROR(ENOMEM);
        JAMI_DBG("No hw frame context");
        return -1;
    }

    // Transfer data from sw frame to hw frame
    if ((err = av_hwframe_transfer_data(hw_frame, sw_frame, 0)) < 0) {
        JAMI_DBG("Error while transferring frame data to surface."
                "Error code: %s.\n", libav_utils::getError(err).c_str());
        return -1;
    }
    // JAMI_WARN("OK transferring frame data to surface");

    // Init packet to receive encoded data
    AVPacket enc_pkt;
    av_init_packet(&enc_pkt);
    enc_pkt.data = NULL;
    enc_pkt.size = 0;

    // Send frame (decoded)
    int ret;
    if ((ret = avcodec_send_frame(avctx, hw_frame)) < 0) {
        JAMI_DBG("Error code: %s\n", libav_utils::getError(ret).c_str());
        ret = ((ret == AVERROR(EAGAIN)) ? 0 : -1);
        return ret;
    }

    // Receive packet (encoded)
    while (1) {
        ret = avcodec_receive_packet(avctx, &enc_pkt);
        if (ret)
            break;

        av_packet_unref(&enc_pkt);
    }
    return 0;
}

int
HardwareAccel::test_device(const HardwareAPI& api, const char* name,
                        const char* device, int flags)
{
    AVBufferRef* hw_device_ctx = nullptr;
    AVHWDeviceContext* dev = nullptr;
    AVCodecContext* avctx = nullptr;
    AVFrame* sw_frame = nullptr;
    AVFrame* hw_frame = nullptr;

    int err;
    err = av_hwdevice_ctx_create(&hw_device_ctx, api.hwType, device, NULL, flags);
    if (err < 0) {
        JAMI_DBG("Failed to create %s device: %d.\n", name, err);
        return 1;
    }

    dev = (AVHWDeviceContext*)hw_device_ctx->data;
    if (dev->type != api.hwType) {
        JAMI_DBG("Device created as type %d has type %d.",
                api.hwType, dev->type);
        av_buffer_unref(&hw_device_ctx);
        return -1;
    }
    JAMI_DBG("Device type %s successfully created.", name);

    
    err = init_codec_ctx(&avctx, hw_device_ctx);
    if (err < 0) {
        JAMI_DBG("Failed to init hardware codec.");
        close_codec_ctx(avctx, hw_device_ctx, nullptr, nullptr);
        return -1;
    }
    else if (err == 0)
        return 0;

    err = test_encode_black_frame(avctx, sw_frame, hw_frame);
    if (err < 0) {
        JAMI_DBG("Failed to encode test frame.");
        return -1;
    }
    close_codec_ctx(avctx, hw_device_ctx, sw_frame, hw_frame);

    return 0;
}

const std::string
HardwareAccel::test_device_type(const HardwareAPI& api)
{
    AVHWDeviceType check;
    const char* name;
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

    JAMI_WARN("-- Starting test for %s with default device.", name);
    err = test_device(api, name, NULL, 0);
    if (err == 0) {
        JAMI_DBG("-- Test passed for %s with default device.", name);
        return "default";
    } else {
        JAMI_DBG("-- Test failed for %s with default device.", name);
    }

    for (unsigned j = 0; j < api.possible_devices.size(); j++) {
        JAMI_WARN("-- Starting test for %s with with device %s.", name, api.possible_devices[j].c_str());
        err = test_device(api, name, api.possible_devices[j].c_str(), 0);
        if (err == 0) {
            JAMI_DBG("-- Test passed for %s with device %s.",
                    name, api.possible_devices[j].c_str());
            return api.possible_devices[j];
        }
        else {
            JAMI_DBG("-- Test failed for %s with device %s.",
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
HardwareAccel::initDevice(const std::string& device)
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
HardwareAccel::setupDecoder(AVCodecID id, int width, int height, AVPixelFormat pix_dec)
{
    for (const auto& api : apiListDec) {
        if (std::find(api.supportedCodecs.begin(), api.supportedCodecs.end(), id) != api.supportedCodecs.end()) {
            auto accel = std::make_unique<HardwareAccel>(id, api.name, api.hwType, api.format, api.swFormat, CODEC_DECODER);
            accel->height_ = height;
            accel->width_ = width;
            accel->fmtDec_ = pix_dec;
            const std::string device = accel->test_device_type(api);
            if(device != "" && accel->initDevice(device)) {
                // we don't need frame context for videotoolbox
                if (api.format == AV_PIX_FMT_VIDEOTOOLBOX || accel->initFrame()) {
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
            accel->height_ = height;
            accel->width_ = width;
            const auto& codecName = accel->getCodecName();
            if (avcodec_find_encoder_by_name(codecName.c_str())) {
                const std::string device = accel->test_device_type(api);
                if(device != "" && accel->initDevice(device)) {
                    bool link = false;
                    if (linkable)
                        link = accel->linkHardware(framesCtx);
                    // we don't need frame context for videotoolbox
                    if (api.format == AV_PIX_FMT_VIDEOTOOLBOX || accel->linkHardware(framesCtx) ||
                       link || accel->initFrame()) {
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
