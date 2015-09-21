/*
 *  Copyright (C) 2012-2015 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "video_provider.h"
#include "video_base.h"

#include <string>
#include <vector>
#include <memory>

namespace ring { namespace video {

#if HAVE_SHM
class ShmHolder;
#endif // HAVE_SHM

class SinkClient : public VideoFramePassiveReader
{
    public:
        SinkClient(const std::string& id="", bool mixer=false);

        const std::string& getId() const noexcept {
            return id_;
        }

        std::string openedName() const noexcept;

        // as VideoFramePassiveReader
        void update(Observable<std::shared_ptr<ring::VideoFrame>>*,
                    std::shared_ptr<ring::VideoFrame>&);

        bool start() noexcept;
        bool stop() noexcept;

        void setFrameSize(int width, int height);

        template <class T>
        void registerTarget(T&& cb) noexcept {
            target_ = std::forward<T>(cb);
        }

    private:
        const std::string id_;
        const bool mixer_;
        std::function<void(std::shared_ptr<std::vector<unsigned char> >&, int, int)> target_;
        std::vector<unsigned char> targetData_;

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
