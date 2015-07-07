/*
 *  Copyright (C) 2013-2015 Savoir-Faire Linux Inc.
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

#ifndef __VIDEO_MIXER_H__
#define __VIDEO_MIXER_H__

#include "noncopyable.h"
#include "video_base.h"
#include "video_scaler.h"
#include "threadloop.h"
#include "rw_mutex.h"

#include <list>
#include <chrono>
#include <memory>

namespace ring { namespace video {

class SinkClient;

    struct VideoMixerSource {
        Observable<std::unique_ptr<QueueFrame>>* source = nullptr;
        std::unique_ptr<QueueFrame> update_frame;
        std::unique_ptr<QueueFrame> render_frame;
        void atomic_swap_render(std::shared_ptr<VideoFrame>& other) {
            /*std::lock_guard<std::mutex> lock(mutex_);
            render_frame.swap(other);
            */
        }
    private:
        std::mutex mutex_ = {};
    };

class VideoMixer :
        public VideoGenerator,
        public VideoFramePassiveReader
{
public:
    VideoMixer(const std::string &id);
    virtual ~VideoMixer();

    void setDimensions(int width, int height);

    int getWidth() const;
    int getHeight() const;
    int getPixelFormat() const;

    // as VideoFramePassiveReader
    void update(Observable<std::unique_ptr<QueueFrame>>*,
                std::unique_ptr<QueueFrame>& frameQueue);
    void attached(Observable<std::unique_ptr<QueueFrame>>* ob);
    void detached(Observable<std::unique_ptr<QueueFrame>>* ob);

private:
    NON_COPYABLE(VideoMixer);

    void render_frame(std::shared_ptr<VideoFrame> output, const VideoFrame& input, int index);

    void start_sink();
    void stop_sink();

    void process();

    const std::string id_;
    int width_ = 0;
    int height_ = 0;
    std::list<VideoMixerSource *> sources_ = {};
    rw_mutex rwMutex_ = {};

    std::shared_ptr<SinkClient> sink_;

    ThreadLoop loop_;
    std::chrono::time_point<std::chrono::system_clock> lastProcess_ = {};
    std::shared_ptr<VideoFrameActiveWriter> videoLocal_ = nullptr;
    VideoScaler scaler_ = {};
};

}} // namespace ring::video

#endif // __VIDEO_MIXER_H__
