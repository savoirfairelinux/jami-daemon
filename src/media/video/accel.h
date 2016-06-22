/*
 *  Copyright (C) 2013-2016 Savoir-faire Linux Inc.
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
        using AccelCreator = HardwareAccel* (*)();

        enum class AccelID {
            NoAccel = 0,
            Vdpau,
            VideoToolbox,
            Dxva2,
            Vaapi,
            Vda,
            Mock
        };

        using AccelInfo = struct AccelInfo {
            enum AccelID type;
            enum AVPixelFormat format;
            std::string name;
            AccelCreator create;
        };

        template <class T>
        static HardwareAccel* make() {
            return new T();
        }

        static const AccelInfo accels[];
        static void setupAccel(AVCodecContext* codecCtx); // checks if codec acceleration is possible
        static HardwareAccel::AccelInfo getAccel(const enum HardwareAccel::AccelID* codecAccels, int size);

        AVPixelFormat format() { return format_; }
        std::string name() { return name_; }
        bool hasFailed() { return fallback_; }

        void setWidth(int width) { width_ = width; }
        void setHeight(int height) { height_ = height; }
        void setProfile(int profile) { profile_ = profile; }

        void preinit(AVCodecContext* codecCtx, AccelInfo info);
        virtual void init(AVCodecContext* codecCtx) = 0;
        virtual int allocateBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags) = 0;
        virtual bool extractData(AVCodecContext* codecCtx, VideoFrame& container) = 0;
        void fail(bool forceFallback = false);

    protected:
        enum AccelID type_;
        AVPixelFormat format_;
        std::string name_;
        int failCount_; // how many failures in a row, reset on success
        bool fallback_; // true when failCount_ exceeds a certain number
        int width_;
        int height_;
        int profile_;
};

static enum AVPixelFormat getFormatCb(AVCodecContext* codecCtx, const enum AVPixelFormat* formats);
static int allocateBufferCb(AVCodecContext* codecCtx, AVFrame* frame, int flags);

#ifdef HAVE_MOCK_ACCEL
class MockAccel : public HardwareAccel {
    public:
        MockAccel();
        ~MockAccel();

        void init(AVCodecContext* codecCtx) override;
        int allocateBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags) override;
        bool extractData(AVCodecContext* codecCtx, VideoFrame& container) override;
};
#endif

}} // namespace ring::video
