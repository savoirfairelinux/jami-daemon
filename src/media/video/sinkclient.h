/*
 *  Copyright (C) 2012-2019 Savoir-faire Linux Inc.
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
#include "media_filter.h"

#include <string>
#include <vector>
#include <memory>

#define DEBUG_FPS

namespace ring { namespace video {

#if HAVE_SHM
class ShmHolder;
#endif // HAVE_SHM

class VideoScaler;

class SinkClient : public VideoFramePassiveReader
{
    public:
        SinkClient(const std::string& id="", bool mixer=false);

        const std::string& getId() const noexcept {
            return id_;
        }

        std::string openedName() const noexcept;

        int getWidth() const noexcept {
            return width_;
        }

        int getHeight() const noexcept {
            return height_;
        }

        // as VideoFramePassiveReader
        void update(Observable<std::shared_ptr<ring::MediaFrame>>*,
                    const std::shared_ptr<ring::MediaFrame>&) override;

        bool start() noexcept;
        bool stop() noexcept;

        /**
          * Set rotation to apply to the video
          * @param rotation Angle to apply
          */
        void setRotation(int rotation);

        void setFrameSize(int width, int height);

        void registerTarget(const DRing::SinkTarget& target) noexcept {
            target_ = target;
        }

    private:
        const std::string id_;
        const bool mixer_;
        int width_ {0};
        int height_ {0};
        bool started_ {false}; // used to arbitrate client's stop signal.
        int rotation_ {0};
        DRing::SinkTarget target_;
        std::unique_ptr<VideoScaler> scaler_;
        std::unique_ptr<MediaFilter> filter_;

#ifdef DEBUG_FPS
        unsigned frameCount_;
        std::chrono::time_point<std::chrono::system_clock> lastFrameDebug_;
#endif

#if HAVE_SHM
        // using shared_ptr and not unique_ptr as ShmHolder is forwared only
        std::shared_ptr<ShmHolder> shm_;
#endif // HAVE_SHM
};

}} // namespace ring::video
