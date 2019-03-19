/*
 *  Copyright (C) 2013-2019 Savoir-faire Linux Inc.
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

#include "noncopyable.h"
#include "video_base.h"
#include "video_scaler.h"
#include "threadloop.h"
#include "rw_mutex.h"
#include "media_filter.h"

#include <list>
#include <chrono>
#include <memory>

namespace ring { namespace video {

class SinkClient;

class VideoMixer:
        public VideoGenerator,
        public VideoFramePassiveReader
{
public:
    VideoMixer(const std::string& id);
    ~VideoMixer();

    void setDimensions(int width, int height);

    int getWidth() const override;
    int getHeight() const override;
    AVPixelFormat getPixelFormat() const override;

    // as VideoFramePassiveReader
    void update(Observable<std::shared_ptr<MediaFrame>>* ob, const std::shared_ptr<MediaFrame>& v) override;
    void attached(Observable<std::shared_ptr<MediaFrame>>* ob) override;
    void detached(Observable<std::shared_ptr<MediaFrame>>* ob) override;

private:
    NON_COPYABLE(VideoMixer);

    struct VideoMixerSource;

    void render_frame(VideoFrame& output, const VideoFrame& input, int index);

    void start_sink();
    void stop_sink();

    void process();

    const std::string id_;
    int width_ = 0;
    int height_ = 0;
    std::list<std::unique_ptr<VideoMixerSource>> sources_;
    rw_mutex rwMutex_;
    int rotation_ = 0;

    std::unique_ptr<MediaFilter> videoRotationFilter_;

    std::shared_ptr<SinkClient> sink_;

    std::chrono::time_point<std::chrono::system_clock> lastProcess_;
    std::shared_ptr<VideoFrameActiveWriter> videoLocal_;
    VideoScaler scaler_;

    ThreadLoop loop_; // as to be last member
};

}} // namespace ring::video
