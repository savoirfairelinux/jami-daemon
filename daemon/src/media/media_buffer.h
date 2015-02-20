/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#pragma once

#include "config.h"

#include <memory>

#ifdef RING_VIDEO
// forward declaration from libav
class AVFrame;
#endif

namespace ring {

#ifdef RING_VIDEO

class VideoFrame {
    public:
        // Construct an empty VideoFrame
        VideoFrame();

        // Return a pointer on underlaying buffer
        AVFrame* pointer() const noexcept { return frame_.get(); }

        void reset() noexcept;

        // Return frame size in bytes
        std::size_t size() const noexcept;

        // Return pixel format
        int format() const noexcept;

        // Return frame width in pixels
        int width() const noexcept;

        // Return frame height in pixels
        int height() const noexcept;

        void reserve(int format, int width, int height);

        void setFromMemory(void* ptr, int format, int width, int height) noexcept;

        // Copy-Assignement
        VideoFrame& operator =(const VideoFrame& src);

    private:
        std::unique_ptr<AVFrame, void(*)(AVFrame*)> frame_;
        bool allocated_ {false};

        void setGeometry(int format, int width, int height) noexcept;
};

// Some helpers
std::size_t videoFrameSize(int format, int width, int height);
void yuv422_clear_to_black(VideoFrame& frame);

#endif // RING_VIDEO

} // namespace ring
