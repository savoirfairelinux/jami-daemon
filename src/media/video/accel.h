/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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
#include "media_buffer.h"
#include "config.h"

#include <string>
#include <memory>

namespace ring { namespace video {

class HardwareAccel {
    public:
        HardwareAccel(const std::string name, const AVPixelFormat format);
        virtual ~HardwareAccel() {};

        AVPixelFormat format() const { return format_; }
        std::string name() const { return name_; }
        bool hasFailed() const { return fallback_; }

        void setCodecCtx(AVCodecContext* codecCtx) { codecCtx_ = codecCtx; }
        void setWidth(int width) { width_ = width; }
        void setHeight(int height) { height_ = height; }
        void setProfile(int profile) { profile_ = profile; }

        void fail(bool forceFallback);
        void succeed() { failCount_ = 0; } // call on success of allocateBuffer or extractData

        bool extractData(VideoFrame& input);

    public: // must be implemented by derived classes
        virtual bool init() = 0;
        virtual int allocateBuffer(AVFrame* frame, int flags) = 0;
        virtual void extractData(VideoFrame& input, VideoFrame& output) = 0;

    protected:
        AVCodecContext* codecCtx_;
        std::string name_;
        AVPixelFormat format_;
        unsigned failCount_; // how many failures in a row, reset on success
        bool fallback_; // true when failCount_ exceeds a certain number
        int width_;
        int height_;
        int profile_;
};

// HardwareAccel factory
// Checks if codec acceleration is possible
std::unique_ptr<HardwareAccel> makeHardwareAccel(AVCodecContext* codecCtx);

}} // namespace ring::video
