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
     * Transfers a hardware decoded frame back to main memory. Should be called after
     * the frame is decoded using avcodec_send_packet/avcodec_receive_frame.
     *
     * @frame: Refrerence to the decoded hardware frame.
     * @returns: Software frame.
     */
    static std::unique_ptr<VideoFrame> transferToMainMemory(const VideoFrame& frame, AVPixelFormat desiredFormat);

    /**
     * Constructs a HardwareAccel object for either decoding or encoding. Should not
     * be called directly. Use HardwareAccelManager.
     */
    HardwareAccel(AVCodecID id, const std::string& name, AVPixelFormat format, AVPixelFormat swFormat, CodecType type);

    /**
     * Dereferences hardware device and hardware frames contexts.
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
     * Decoding: avcodec_get_name(id_)
     * Encoding: avcodec_get_name(id_) + '_' + name_
     */
    std::string getCodecName() const;

    /**
     * Returns whether or not the decoder is linked to an encoder or vice-versa. Being linked
     * means an encoder can directly use the decoder's hardware frame, without first
     * transferring it to main memory.
     */
    bool isLinked() const { return linked_; }

    /**
     * Set some extra details in the codec context. Should be called after a successful
     * setup (setupDecoder or setupEncoder).
     * For decoding, sets the hw_device_ctx and get_format callback. If the hardware
     * decoder has a frames context, mark as linked. For encoding, sets hw_frames_ctx,
     * and may set some hardware specific options in the dictionary.
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
    bool linked_ {false};

    AVBufferRef* deviceCtx_ {nullptr};
    AVBufferRef* framesCtx_ {nullptr};

    /**
     * Manager needs to access initDevice and initFrame.
     */
    friend class HardwareAccelManager;
};

class HardwareAccelManager {
public:
    /**
     * Single instance makes it easier to have one accel reference another, which is needed
     * for decoding and encoding without having the main memory act as a middle man.
     */
    static HardwareAccelManager& instance();

    /**
     * Factory method for hardware decoding.
     */
    std::shared_ptr<HardwareAccel> setupDecoder(AVCodecID id, int width, int height);

    /**
     * Factory method for hardware encoding.
     */
    std::shared_ptr<HardwareAccel> setupEncoder(AVCodecID id, int width, int height);

    /**
     * Links @enc's hardware frame context to the hardware decoder with codec @decoderId.
     * This serves to skip transferring a decoded frame back to main memory before encoding.
     */
    bool linkHardware(std::shared_ptr<HardwareAccel>& enc, AVCodecID id);

private:
    HardwareAccelManager();
    ~HardwareAccelManager();

    std::vector<std::weak_ptr<HardwareAccel>> decoders_;
};

}} // namespace ring::video
