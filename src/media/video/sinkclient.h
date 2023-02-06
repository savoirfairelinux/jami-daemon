/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "video_base.h"
#include <videomanager_interface.h>

#include <string>
#include <vector>
#include <memory>

#define DEBUG_FPS

namespace jami {
class MediaFilter;
}

namespace jami {
namespace video {

#ifdef ENABLE_SHM
class ShmHolder;
#endif // ENABLE_SHM

class VideoScaler;

class SinkClient : public VideoFramePassiveReader, public VideoFrameActiveWriter
{
public:
    SinkClient(const std::string& id = "", bool mixer = false);

    const std::string& getId() const noexcept { return id_; }

    std::string openedName() const noexcept;

    int getWidth() const noexcept { return width_; }

    int getHeight() const noexcept { return height_; }

    AVPixelFormat getPreferredFormat() const noexcept
    {
        return (AVPixelFormat) target_.preferredFormat;
    }

    // as VideoFramePassiveReader
    void update(Observable<std::shared_ptr<jami::MediaFrame>>*,
                const std::shared_ptr<jami::MediaFrame>&) override;

    bool start() noexcept;
    bool stop() noexcept;

    void setFrameSize(int width, int height);
    void setCrop(int x, int y, int w, int h);

    void registerTarget(libjami::SinkTarget target) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        target_ = std::move(target);
    }

#ifdef ENABLE_SHM
    void enableShm(bool value) { doShmTransfer_.store(value); }
#endif

private:
    const std::string id_;
    // True if the instance is used by a mixer.
    const bool mixer_ {false};
    int width_ {0};
    int height_ {0};

    struct Rect
    {
        int x {0}, y {0}, w {0}, h {0};
    };
    Rect crop_ {};

    bool started_ {false}; // used to arbitrate client's stop signal.
    int rotation_ {0};
    libjami::SinkTarget target_;
    std::unique_ptr<VideoScaler> scaler_;
    std::unique_ptr<MediaFilter> filter_;
    std::mutex mtx_;

    void sendFrameDirect(const std::shared_ptr<jami::MediaFrame>&);
    void sendFrameTransformed(AVFrame* frame);

    /**
     * Apply required transformations before sending frames to clients/observers:
     * - Transfer the frame from gpu to main memory, if needed.
     * - Rotate the frame as needed.
     * - Apply cropping as needed
     */
    std::shared_ptr<VideoFrame> applyTransform(VideoFrame& frame);

#ifdef DEBUG_FPS
    unsigned frameCount_;
    std::chrono::steady_clock::time_point lastFrameDebug_;
#endif

#ifdef ENABLE_SHM
    // using shared_ptr and not unique_ptr as ShmHolder is forwared only
    std::shared_ptr<ShmHolder> shm_;
    std::atomic_bool doShmTransfer_ {false};
#endif // ENABLE_SHM
};

} // namespace video
} // namespace jami
