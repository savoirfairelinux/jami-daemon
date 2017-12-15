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

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "config.h"

#ifdef RING_VIDEOTOOLBOX

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavcodec/videotoolbox.h>
#include <libavutil/imgutils.h>
}

#include "video/accel.h"

#include <memory>
#include <functional>

namespace ring { namespace video {

class VideoToolboxAccel : public HardwareAccel {
    public:
        VideoToolboxAccel(const std::string name, const AVPixelFormat format);
        ~VideoToolboxAccel();

        bool checkAvailability() override;
        bool init() override;
        int allocateBuffer(AVFrame* frame, int flags) override;
        void extractData(VideoFrame& input, VideoFrame& output) override;

    private:
        using AVBufferRefPtr = std::unique_ptr<AVBufferRef, std::function<void(AVBufferRef*)>>;
        AVBufferRefPtr deviceBufferRef_;
};

}} // namespace ring::video

#endif // RING_VIDEOTOOLBOX
