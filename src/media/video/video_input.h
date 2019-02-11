/*
 *  Copyright (C) 2011-2019 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
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

#ifndef __VIDEO_INPUT_H__
#define __VIDEO_INPUT_H__

#include "noncopyable.h"
#include "threadloop.h"
#include "media_stream.h"
#include "media/media_device.h" // DeviceParams
#include "media/video/video_base.h"

#include <map>
#include <atomic>
#include <future>
#include <string>
#include <mutex>
#include <condition_variable>
#include <array>

#if __APPLE__
#import "TargetConditionals.h"
#endif

namespace ring {
class MediaDecoder;
}

namespace ring { namespace video {

class SinkClient;

enum VideoFrameStatus {
    BUFFER_NOT_ALLOCATED,
    BUFFER_AVAILABLE,       /* owned by us */
    BUFFER_CAPTURING,       /* owned by Android Java Application */
    BUFFER_FULL,            /* owned by us again */
    BUFFER_PUBLISHED,       /* owned by libav */
};

struct VideoFrameBuffer {
    void                    *data;
    size_t                  length;
    enum VideoFrameStatus   status;
    int                     index;

    VideoFrameBuffer() : data(nullptr), length(0),
                         status(BUFFER_NOT_ALLOCATED), index(0) {}
};

class VideoInput : public VideoGenerator, public std::enable_shared_from_this<VideoInput>
{
public:
    VideoInput();
    ~VideoInput();

    // as VideoGenerator
    int getWidth() const;
    int getHeight() const;
    AVPixelFormat getPixelFormat() const;
    DeviceParams getParams() const;
    MediaStream getInfo() const;

    std::shared_future<DeviceParams> switchInput(const std::string& resource);
#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    /*
     * these functions are used to pass buffer from/to the daemon
     * on the Android and UWP builds
     */
    void* obtainFrame(int length);
    void releaseFrame(void *frame);
#endif

private:
    NON_COPYABLE(VideoInput);

    std::string currentResource_;

    std::unique_ptr<MediaDecoder> decoder_;
    std::shared_ptr<SinkClient> sink_;
    std::atomic<bool> switchPending_ = {false};

    DeviceParams decOpts_;
    std::promise<DeviceParams> foundDecOpts_;
    std::shared_future<DeviceParams> futureDecOpts_;

    std::atomic_bool decOptsFound_ {false};
    void foundDecOpts(const DeviceParams& params);

    bool emulateRate_       = false;
    ThreadLoop loop_;

    void clearOptions();

    void createDecoder();
    void deleteDecoder();

    // true if decOpts_ is ready to use, false if using promise/future
    bool initCamera(const std::string& device);
    bool initX11(std::string display);
    bool initAVFoundation(const std::string& display);
    bool initFile(std::string path);
    bool initGdiGrab(std::string params);

    // for ThreadLoop
    bool setup();
    void process();
    void cleanup();

    bool captureFrame();
    bool isCapturing() const noexcept;

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    int allocateOneBuffer(struct VideoFrameBuffer& b, int length);
    void freeOneBuffer(struct VideoFrameBuffer& b);
    bool waitForBufferFull();

    std::mutex mutex_;
    std::condition_variable frame_cv_;
    int capture_index_ = 0;
    int publish_index_ = 0;

    /* Get notified when libav is done with this buffer */
    void releaseBufferCb(uint8_t* ptr);
    std::array<struct VideoFrameBuffer, 8> buffers_;
#endif
};

}} // namespace ring::video

#endif // __VIDEO_INPUT_H__
