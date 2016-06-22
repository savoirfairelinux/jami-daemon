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

class HardwareAccel;

enum class AccelID {
    NoAccel = 0,
    Vdpau,
    VideoToolbox,
    Dxva2,
    Vaapi,
    Vda
};

struct AccelInfo {
    AccelID type;
    AVPixelFormat format;
    std::string name;
    std::unique_ptr<HardwareAccel> (*create)(const AccelInfo& info);
};

class HardwareAccel {
    public:
        HardwareAccel(const AccelInfo& info);
        virtual ~HardwareAccel() {};

        AVPixelFormat format() const { return format_; }
        std::string name() const { return name_; }
        bool hasFailed() const { return fallback_; }

        void setWidth(int width) { width_ = width; }
        void setHeight(int height) { height_ = height; }
        void setProfile(int profile) { profile_ = profile; }

        virtual bool init(AVCodecContext* codecCtx) = 0;
        virtual int allocateBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags) = 0;
        virtual bool extractData(AVCodecContext* codecCtx, VideoFrame& container) = 0;
        void fail(bool forceFallback = false);
        void succeed() { failCount_ = 0; } // call on success of allocateBuffer or extractData

    protected:
        AccelID type_;
        AVPixelFormat format_;
        std::string name_;
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
