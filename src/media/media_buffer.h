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
#include "ring_types.h"

#include <memory>
#include <queue>

class AVFrame;

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
        void setFromMemory(void* ptr, int format, int width, int height) noexcept;

        void noise();

        // Copy-Assignement
        VideoFrame& operator =(const VideoFrame& src);

    private:
        bool allocated_ {false};
        void setGeometry(int format, int width, int height) noexcept;
};

// Some helpers
std::size_t videoFrameSize(int format, int width, int height);
void yuv422_clear_to_black(VideoFrame& frame);

//TODO : declare better !
typedef std::queue<std::shared_ptr<VideoFrame>> QueueFrame;
struct QueueFrameInfo
{
    QueueFrameInfo();
    ~QueueFrameInfo();
    std::unique_ptr<QueueFrame> queueVideo;
    std::unique_ptr<QueueFrame> queueAudio;
    const unsigned QUEUE_VIDEO_MAX_SIZE = 300;
    const unsigned QUEUE_AUDIO_MAX_SIZE = 0;
    //std::shared_ptr<VideoFrame> popVideoFrame();
    bool pushVideoFrame(std::shared_ptr<VideoFrame> videoFrame);
    std::mutex queueMutex;
};

extern decltype(getGlobalInstance<QueueFrameInfo>)& getQueueFrame;
/*std::unique_ptr<QueueFrameInfo> my_queueFrameInfo (new QueueFrameInfo);
std::unique_ptr<QueueFrameInfo> getQueueFrame() {
    return my_queueFrameInfo;
}*/


#endif // RING_VIDEO

} // namespace ring
