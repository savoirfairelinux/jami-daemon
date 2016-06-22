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

 /* File borrowed and adapted from FFmpeg*/

#pragma once

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include <map>
#include <memory>

#include "config.h"

#define RING_VAAPI "vaapi"

namespace ring { namespace video {

class HardwareAccel {
    public:
        HardwareAccel() {};
        ~HardwareAccel() {};

        const std::string name() { return name_; }
        const enum AVPixelFormat pixelFormat() { return pixFmt_; }

        virtual void init(AVCodecContext* codecCtx) = 0;
        virtual int getBuffer(AVFrame* frame, int flags) = 0;
        virtual int retrieveData(AVFrame* frame) = 0;

    protected:
        std::string name_;
        AVPixelFormat pixFmt_;
        AVCodecContext* codecCtx_ = nullptr;
};

class HardwareAccelContext {
    public:
        HardwareAccelContext() {};
        ~HardwareAccelContext() {};

        int autoDetect() { return autoDetect_; }
        int success() { return autoDetect_ || pixFmt_ != AV_PIX_FMT_NONE; }
        void findHardwareAccel(AVCodecContext* codecCtx, std::string hardwareAccelName);
        int retrieveData(AVFrame* frame);

        static std::map<std::string, AVPixelFormat> pixelFormatMap_;
        using HardwareAccelCreator = HardwareAccel* (*)();
        template <typename T>
        static HardwareAccel* make() { return new T{}; }
        static std::map<AVPixelFormat, HardwareAccelCreator> hardwareAccelMap_;

    private:
        AVCodecContext* codecCtx_ = nullptr;
        HardwareAccel* hardwareAccel_ = nullptr;
        AVPixelFormat pixFmt_ = AV_PIX_FMT_NONE;
        int autoDetect_ = 0;

        enum AVPixelFormat getFormat(const enum AVPixelFormat* pixFmts);
        int getBuffer(AVFrame* frame, int flags);

        static int getBufferCb(AVCodecContext* ctx, AVFrame* frame, int flags)
        {
            auto thisPtr = static_cast<HardwareAccelContext*>(ctx->opaque);
            return thisPtr->getBuffer(frame, flags);
        }
        static enum AVPixelFormat getFormatCb(AVCodecContext* ctx, const enum AVPixelFormat* pixFmt)
        {
            auto thisPtr = static_cast<HardwareAccelContext*>(ctx->opaque);
            return thisPtr->getFormat(pixFmt);
        }
};

}} // namespace ring::video
