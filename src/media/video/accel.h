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
    static std::unique_ptr<HardwareAccel> setupDecoder(AVCodecContext* codecCtx);

    /**
     * Transfers a hardware decoded frame back to main memory. Should be called after
     * the frame is decoded using avcodec_send_packet/avcodec_receive_frame.
     *
     * @frame: Refrerence to the decoded hardware frame.
     * @returns: Software frame.
     */
    static std::unique_ptr<VideoFrame> transferToMainMemory(const VideoFrame& frame, AVPixelFormat desiredFormat);

    /**
     * Made public so std::unique_ptr can access it. Should not be called.
     */
    HardwareAccel(AVCodecID id, const std::string& name, AVPixelFormat format);

    AVCodecID getCodecId() const { return id_; };
    std::string getName() const { return name_; };
    AVPixelFormat getFormat() const { return format_; };

    /**
     * Transfers a hardware decoded frame back to main memory. Should be called after
     * the frame is decoded using avcodec_send_packet/avcodec_receive_frame.
     *
     * @frame: Refrerence to the decoded hardware frame.
     * @returns: Software frame.
     */
    std::unique_ptr<VideoFrame> transfer(const VideoFrame& frame);

private:
    AVCodecID id_;
    std::string name_;
    AVPixelFormat format_;
};

}} // namespace ring::video
