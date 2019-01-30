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

#pragma once

#include "libav_deps.h"
#include "media_codec.h"

#include <memory>
#include <string>
#include <vector>

namespace ring { namespace video {

/**
 * Provides an abstraction layer to the hardware acceleration APIs in FFmpeg.
 */
class HardwareAccel {
public:
    /**
     * Static factory method for hardware decoding.
     */
    static std::unique_ptr<HardwareAccel> setupDecoder(AVCodecID id);

    /**
     * Static factory method for hardware encoding.
     */
    static std::unique_ptr<HardwareAccel> setupEncoder(AVCodecID id, int width, int height);

    /**
     * Made public so std::unique_ptr can access it. Should not be called.
     */
    HardwareAccel(AVCodecID id, const std::string& name, AVPixelFormat format, AVPixelFormat swFormat, CodecType type);

    /**
     * Dereferences hardware contexts.
     */
    ~HardwareAccel();

    /**
     * Codec that is being accelerated.
     */
    AVCodecID getCodecId() const { return id_; };

    /**
     * Name of the hardware layer/API being used.
     */
    std::string getName() const { return name_; };

    /**
     * Hardware format.
     */
    AVPixelFormat getFormat() const { return format_; };

    /**
     * Software format. For encoding it is the format expected by the hardware. For decoding
     * it is the format output by the hardware.
     */
    AVPixelFormat getSoftwareFormat() const { return swFormat_; }

    /**
     * Gets the name of the codec.
     * Decoding: equivalent to avcodec_get_name(id_)
     * Encoding: avcodec_get_name(id_) + '_' + name_
     */
    std::string getCodecName() const;

    /**
     * Set some extra details in the codec context. Should be called after a successful
     * setup (setupDecoder or setupEncoder).
     * For decoding, sets the hw_device_ctx and get_format callback. For encoding, sets
     * hw_device_ctx and hw_frames_ctx, and may set some hardware specific options in
     * the dictionary.
     */
    void setDetails(AVCodecContext* codecCtx, AVDictionary** d);

    /**
     * Transfers a hardware decoded frame back to main memory. Should be called after
     * the frame is decoded using avcodec_send_packet/avcodec_receive_frame or before
     * the frame is encoded using avcodec_send_frame/avcodec_receive_packet.
     *
     * @frame: Hardware frame when decoding, software frame when encoding.
     * @returns: Software frame when decoding, hardware frame when encoding.
     */
    std::unique_ptr<VideoFrame> transfer(const VideoFrame& frame);

private:
    bool initDevice();
    bool initFrame(int width, int height);

    AVCodecID id_ {AV_CODEC_ID_NONE};
    std::string name_;
    AVPixelFormat format_ {AV_PIX_FMT_NONE};
    AVPixelFormat swFormat_ {AV_PIX_FMT_NONE};
    CodecType type_ {CODEC_NONE};

    AVBufferRef* deviceCtx_ {nullptr};
    AVBufferRef* framesCtx_ {nullptr};
};

}} // namespace ring::video
