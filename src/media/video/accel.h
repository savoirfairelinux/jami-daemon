/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
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
        HardwareAccel(const std::string& name, const AVPixelFormat format);
        virtual ~HardwareAccel() {};

        AVPixelFormat format() const { return format_; }
        std::string name() const { return name_; }
        bool hasFailed() const { return fallback_; }

        void setCodecCtx(AVCodecContext* codecCtx) { codecCtx_ = codecCtx; }
        void setWidth(int width) { width_ = width; }
        void setHeight(int height) { height_ = height; }
        void setProfile(int profile) { profile_ = profile; }

        void failAllocation();
        void failExtraction();
        void fail(bool forceFallback);
        void succeedAllocation() { allocationFails_ = 0; }
        void succeedExtraction() { extractionFails_ = 0; }

        // wrapper to take care of boilerplate before calling the derived class's implementation
        bool extractData(VideoFrame& input);

    public: // must be implemented by derived classes
        virtual bool checkAvailability() = 0;
        virtual bool init() = 0;
        virtual int allocateBuffer(AVFrame* frame, int flags) = 0;
        virtual void extractData(VideoFrame& input, VideoFrame& output) = 0;

    protected:
        AVCodecContext* codecCtx_ = nullptr;
        std::string name_;
        AVPixelFormat format_;
        unsigned allocationFails_ = 0; // how many times in a row allocateBuffer has failed
        unsigned extractionFails_ = 0; // how many times in a row extractData has failed
        bool fallback_ = false; // set to true when successive failures exceeds MAX_ACCEL_FAILURES
        int width_ = -1;
        int height_ = -1;
        int profile_ = -1;
};

// HardwareAccel factory
// Checks if codec acceleration is possible
std::unique_ptr<HardwareAccel> makeHardwareAccel(AVCodecContext* codecCtx);

}} // namespace ring::video
