/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
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

#pragma once

#include "noncopyable.h"
#include "threadloop.h"
#include "media_stream.h"
#include "media/media_device.h" // DeviceParams
#include "media/video/video_base.h"
#include "media_codec.h"

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

namespace jami {
class MediaDecoder;
class MediaDemuxer;
} // namespace jami

namespace jami {
namespace video {

class SinkClient;

enum class VideoInputMode { ManagedByClient, ManagedByDaemon, Undefined };

class VideoInput : public VideoGenerator, public std::enable_shared_from_this<VideoInput>
{
public:
    VideoInput(VideoInputMode inputMode = VideoInputMode::Undefined,
               const std::string& id_ = "local");
    ~VideoInput();

    // as VideoGenerator
    const std::string& getName() const { return currentResource_; }
    int getWidth() const;
    int getHeight() const;
    AVPixelFormat getPixelFormat() const;

    const DeviceParams& getConfig() const {
        return decOpts_;
    }
    std::shared_future<DeviceParams> getParams() const {
        return futureDecOpts_;
    }

    MediaStream getInfo() const;

    void setSink(const std::string& sinkId);
    void updateStartTime(int64_t startTime);
    void configureFilePlayback(const std::string& path,
                               std::shared_ptr<MediaDemuxer>& demuxer,
                               int index);
    void flushBuffers();
    void setPaused(bool paused) { paused_ = paused; }
    void setSeekTime(int64_t time);
    void setFrameSize(const int width, const int height);
    void setupSink();
    void stopSink();

    void setRecorderCallback(const std::function<void(VideoFrameActiveWriter* vg, const MediaStream& ms)>& cb);

#if VIDEO_CLIENT_INPUT
    /*
     * these functions are used to pass buffer from/to the daemon
     * on the Android and UWP builds
     */
    void* obtainFrame(int length);
    void releaseFrame(void* frame);
#else
    void stopInput();
#endif

    void setSuccessfulSetupCb(const std::function<void(MediaType, bool)>& cb)
    {
        onSuccessfulSetup_ = cb;
    }

    /**
     * Restart stopped video input
     * @note if a media is removed, then re-added in a conference, the loop will be stopped
     * and this input must be restarted
     */
    void restart();

private:
    NON_COPYABLE(VideoInput);

    std::shared_future<DeviceParams> switchInput(const std::string& resource);

    std::string id_;
    std::string currentResource_;
    std::atomic<bool> switchPending_ = {false};
    std::atomic_bool isStopped_ = {false};

    DeviceParams decOpts_;
    std::promise<DeviceParams> foundDecOpts_;
    std::shared_future<DeviceParams> futureDecOpts_;
    bool emulateRate_ = false;

    std::atomic_bool decOptsFound_ {false};
    void foundDecOpts(const DeviceParams& params);

    void clearOptions();

    // true if decOpts_ is ready to use, false if using promise/future
    bool initCamera(const std::string& device);
    bool initX11(const std::string& display);
    bool initAVFoundation(const std::string& display);
    bool initFile(std::string path);
#ifdef WIN32
    bool initWindowsGrab(const std::string& display);
#endif

    bool isCapturing() const noexcept;
    void startLoop();

    void switchDevice();
    bool capturing_ {false};
    void createDecoder();
    void deleteDecoder();
    std::unique_ptr<MediaDecoder> decoder_;
    std::shared_ptr<SinkClient> sink_;
    ThreadLoop loop_;

    // for ThreadLoop
    bool setup();
    void process();
    void cleanup();

    bool captureFrame();

    int rotation_ {0};
    std::shared_ptr<AVBufferRef> displayMatrix_;
    void setRotation(int angle);
    VideoInputMode inputMode_;
    inline bool videoManagedByClient() const
    {
        return inputMode_ == VideoInputMode::ManagedByClient;
    }
    bool playingFile_ = false;
    std::atomic_bool paused_ {true};

    std::function<void(MediaType, bool)> onSuccessfulSetup_;
    std::function<void(VideoFrameActiveWriter* vg, const MediaStream& ms)> recorderCallback_;
};

} // namespace video
} // namespace jami
