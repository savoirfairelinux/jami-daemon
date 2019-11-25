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

#pragma once

#include "libav_deps.h"
#include "media_codec.h"

#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <libavutil/hwcontext.h>
}

namespace jami { namespace video {

struct HardwareAPI
{
    std::string name;
    AVHWDeviceType hwType;
    AVPixelFormat format;
    AVPixelFormat swFormat;
    std::vector<AVCodecID> supportedCodecs;
    std::vector<std::string> possible_devices;
};

/**
 * @brief Provides an abstraction layer to the hardware acceleration APIs in FFmpeg.
 */
class HardwareAccel {
public:
    /**
     * @brief Static factory method for hardware decoding.
     */
    static std::unique_ptr<HardwareAccel> setupDecoder(AVCodecID id, int width, int height);

    /**
     * @brief Static factory method for hardware encoding.
     */
    static std::unique_ptr<HardwareAccel> setupEncoder(AVCodecID id, int width, int height, bool linkable,
        AVBufferRef* framesCtx = nullptr);

    /**
     * @brief Transfers hardware frame to main memory.
     *
     * Transfers a hardware decoded frame back to main memory. Should be called after
     * the frame is decoded using avcodec_send_packet/avcodec_receive_frame.
     *
     * If @frame is software, this is a no-op.
     *
     * @param frame Refrerence to the decoded hardware frame.
     * @param desiredFormat Software pixel format that the hardware outputs.
     * @returns Software frame.
     */
    static std::unique_ptr<VideoFrame> transferToMainMemory(const VideoFrame& frame, AVPixelFormat desiredFormat);

    /**
     * @brief Constructs a HardwareAccel object
     *
     * Made public so std::unique_ptr can access it. Should not be called.
     */
    HardwareAccel(AVCodecID id, const std::string& name, AVHWDeviceType hwType, AVPixelFormat format, AVPixelFormat swFormat, CodecType type);

    /**
     * @brief Dereferences hardware contexts.
     */
    ~HardwareAccel();

    /**
     * @brief Codec that is being accelerated.
     */
    AVCodecID getCodecId() const { return id_; };

    /**
     * @brief Name of the hardware layer/API being used.
     */
    std::string getName() const { return name_; };

    /**
     * @brief Hardware format.
     */
    AVPixelFormat getFormat() const { return format_; };

    /**
     * @brief Software format.
     *
     * For encoding it is the format expected by the hardware. For decoding
     * it is the format output by the hardware.
     */
    AVPixelFormat getSoftwareFormat() const { return swFormat_; }

    /**
     * @brief Gets the name of the codec.
     *
     * Decoding: avcodec_get_name(id_)
     * Encoding: avcodec_get_name(id_) + '_' + name_
     */
    std::string getCodecName() const;

    /**
     * @brief If hardware decoder can feed hardware encoder directly.
     *
     * Returns whether or not the decoder is linked to an encoder or vice-versa. Being linked
     * means an encoder can directly use the decoder's hardware frame, without first
     * transferring it to main memory.
     */
    bool isLinked() const { return linked_; }

    /**
     * @brief Set some extra details in the codec context.
     *
     * Should be called after a successful
     * setup (setupDecoder or setupEncoder).
     * For decoding, sets the hw_device_ctx and get_format callback. If the decoder has
     * a frames context, mark as linked.
     * For encoding, sets hw_device_ctx and hw_frames_ctx, and may set some hardware
     * codec options.
     */
    void setDetails(AVCodecContext* codecCtx);

    /**
     * @brief Transfers a frame to/from the GPU memory.
     *
     * Transfers a hardware decoded frame back to main memory. Should be called after
     * the frame is decoded using avcodec_send_packet/avcodec_receive_frame or before
     * the frame is encoded using avcodec_send_frame/avcodec_receive_packet.
     *
     * @param frame Hardware frame when decoding, software frame when encoding.
     * @returns Software frame when decoding, hardware frame when encoding.
     */
    std::unique_ptr<VideoFrame> transfer(const VideoFrame& frame);

    /**
     * @brief Links this HardwareAccel's frames context with the passed in context.
     *
     * This serves to skip transferring a decoded frame back to main memory before encoding.
     */
    bool linkHardware(AVBufferRef* framesCtx);

private:
    bool initDevice(std::string device);
    bool initFrame(int width, int height);

    AVCodecID id_ {AV_CODEC_ID_NONE};
    std::string name_;
    AVHWDeviceType hwType_ {AV_HWDEVICE_TYPE_NONE};
    AVPixelFormat format_ {AV_PIX_FMT_NONE};
    AVPixelFormat swFormat_ {AV_PIX_FMT_NONE};
    CodecType type_ {CODEC_NONE};
    bool linked_ {false};

    AVBufferRef* deviceCtx_ {nullptr};
    AVBufferRef* framesCtx_ {nullptr};

    static int test_device(const HardwareAPI& api, const char* name,
                        const char* device, int flags);
    static std::string test_device_type(const HardwareAPI& api);
};

}} // namespace jami::video
