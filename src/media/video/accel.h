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

#define MAX_ACCEL_FAILURES 5

namespace ring { namespace video {

class HardwareAccel {
    public:
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
            std::unique_ptr<HardwareAccel> (*create)(AccelInfo info);
        };

        template <class T>
        static std::unique_ptr<HardwareAccel> make(AccelInfo info) {
            return std::unique_ptr<HardwareAccel>(new T(info));
        }

        static std::unique_ptr<HardwareAccel> setupAccel(AVCodecContext* codecCtx); // checks if codec acceleration is possible
        static const HardwareAccel::AccelInfo getAccel(const HardwareAccel::AccelID* codecAccels, int size);

        HardwareAccel(AccelInfo info);

        AVPixelFormat format() const { return format_; }
        std::string name() const { return name_; }
        bool hasFailed() const { return fallback_; }

        void setWidth(int width) { width_ = width; }
        void setHeight(int height) { height_ = height; }
        void setProfile(int profile) { profile_ = profile; }

        virtual void init(AVCodecContext* codecCtx) = 0;
        virtual int allocateBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags) = 0;
        virtual bool extractData(AVCodecContext* codecCtx, VideoFrame& container) = 0;
        void fail(bool forceFallback = false);
        void reset() { failCount_ = 0; } // call on success of allocateBuffer or extractData

    protected:
        AccelID type_;
        AVPixelFormat format_;
        std::string name_;
        int failCount_; // how many failures in a row, reset on success
        bool fallback_; // true when failCount_ exceeds a certain number
        int width_;
        int height_;
        int profile_;
};

static AVPixelFormat getFormatCb(AVCodecContext* codecCtx, const AVPixelFormat* formats);
static int allocateBufferCb(AVCodecContext* codecCtx, AVFrame* frame, int flags);

}} // namespace ring::video
