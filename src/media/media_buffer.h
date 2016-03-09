/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#include "config.h"

#include <memory>

class AVFrame;

namespace DRing {
class FrameBuffer; //  from dring/videomanager_interface.h
}

namespace ring {

class MediaFrame {
    public:
        // Construct an empty MediaFrame
        MediaFrame();

        virtual ~MediaFrame() = default;

        // Return a pointer on underlaying buffer
        AVFrame* pointer() const noexcept { return frame_.get(); }

        // Reset internal buffers (return to an empty MediaFrame)
        virtual void reset() noexcept;

    protected:
        std::unique_ptr<AVFrame, void(*)(AVFrame*)> frame_;
};

struct AudioFrame: MediaFrame {};

#ifdef RING_VIDEO

class VideoFrame: public MediaFrame {
    public:
        // Construct an empty VideoFrame
        VideoFrame() = default;
        ~VideoFrame();

        // Reset internal buffers (return to an empty VideoFrame)
        void reset() noexcept override;

        // Return frame size in bytes
        std::size_t size() const noexcept;

        // Return pixel format
        int format() const noexcept;

        // Return frame width in pixels
        int width() const noexcept;

        // Return frame height in pixels
        int height() const noexcept;

        // Allocate internal pixel buffers following given specifications
        void reserve(int format, int width, int height);

        // Set internal pixel buffers on given memory buffer
        // This buffer must follow given specifications.
        void setFromMemory(uint8_t* data, int format, int width, int height) noexcept;
        void setFromMemory(uint8_t* data, int format, int width, int height, std::function<void(uint8_t*)> cb) noexcept;

        void noise();

        // Copy-Assignement
        VideoFrame& operator =(const VideoFrame& src);

    private:
        std::function<void(uint8_t*)> releaseBufferCb_ {};
        uint8_t* ptr_ {nullptr};
        bool allocated_ {false};
        void setGeometry(int format, int width, int height) noexcept;
};

// Some helpers
std::size_t videoFrameSize(int format, int width, int height);
void yuv422_clear_to_black(VideoFrame& frame);

#endif // RING_VIDEO

} // namespace ring
