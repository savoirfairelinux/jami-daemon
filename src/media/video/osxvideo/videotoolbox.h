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

#if defined(RING_VIDEOTOOLBOX) || defined(RING_VDA)

extern "C" {
#include <libavcodec/avcodec.h>
#ifdef RING_VIDEOTOOLBOX
#include <libavcodec/videotoolbox.h>
#endif
#ifdef RING_VDA
#include <libavcodec/vda.h>
#endif
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
        bool usingVT_ = false;
        std::string decoderName_;
};

}} // namespace ring::video

#endif // defined(RING_VIDEOTOOLBOX) || defined(RING_VDA)
