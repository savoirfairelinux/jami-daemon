/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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
#include "media_stream.h"

#include <list>
#include <chrono>
#include <memory>

namespace jami {
namespace video {

class SinkClient;

struct SourceInfo
{
    Observable<std::shared_ptr<MediaFrame>>* source;
    int x;
    int y;
    int w;
    int h;
    bool hasVideo;
};
using OnSourcesUpdatedCb = std::function<void(std::vector<SourceInfo>&&)>;

enum class Layout { GRID, ONE_BIG_WITH_SMALL, ONE_BIG };

class VideoMixer : public VideoGenerator, public VideoFramePassiveReader
{
public:
    VideoMixer(const std::string& id, const std::string& localInput = {});
    ~VideoMixer();

    void setParameters(int width, int height, AVPixelFormat format = AV_PIX_FMT_YUV422P);

    int getWidth() const override;
    int getHeight() const override;
    AVPixelFormat getPixelFormat() const override;

    // as VideoFramePassiveReader
    void update(Observable<std::shared_ptr<MediaFrame>>* ob,
                const std::shared_ptr<MediaFrame>& v) override;
    void attached(Observable<std::shared_ptr<MediaFrame>>* ob) override;
    void detached(Observable<std::shared_ptr<MediaFrame>>* ob) override;

    void switchInput(const std::string& input);
    void switchSecondaryInput(const std::string& input);
    void stopInput();

    void setActiveParticipant(Observable<std::shared_ptr<MediaFrame>>* ob);
    void setActiveHost();

    Observable<std::shared_ptr<MediaFrame>>* getActiveParticipant() { return activeSource_; }

    void setVideoLayout(Layout newLayout)
    {
        currentLayout_ = newLayout;
        layoutUpdated_ += 1;
    }

    void setOnSourcesUpdated(OnSourcesUpdatedCb&& cb) { onSourcesUpdated_ = std::move(cb); }

    MediaStream getStream(const std::string& name) const;

    std::shared_ptr<VideoFrameActiveWriter>& getVideoLocal() { return videoLocal_; }

    void updateLayout();

private:
    NON_COPYABLE(VideoMixer);
    struct VideoMixerSource;

    bool render_frame(VideoFrame& output,
                      const std::shared_ptr<VideoFrame>& input,
                      std::unique_ptr<VideoMixerSource>& source);

    void calc_position(std::unique_ptr<VideoMixerSource>& source,
                       const std::shared_ptr<VideoFrame>& input,
                       int index);

    void start_sink();
    void stop_sink();

    void process();

    const std::string id_;
    int width_ = 0;
    int height_ = 0;
    AVPixelFormat format_ = AV_PIX_FMT_YUV422P;
    rw_mutex rwMutex_;

    std::shared_ptr<SinkClient> sink_;

    std::chrono::time_point<std::chrono::steady_clock> nextProcess_;
    std::shared_ptr<VideoFrameActiveWriter> videoLocal_;
    std::shared_ptr<VideoFrameActiveWriter> videoLocalSecondary_;

    VideoScaler scaler_;

    ThreadLoop loop_; // as to be last member

    Layout currentLayout_ {Layout::GRID};
    Observable<std::shared_ptr<MediaFrame>>* activeSource_ {nullptr};
    std::list<std::unique_ptr<VideoMixerSource>> sources_;

    std::atomic_int layoutUpdated_ {0};
    OnSourcesUpdatedCb onSourcesUpdated_ {};

    int64_t startTime_;
    int64_t lastTimestamp_;
};

} // namespace video
} // namespace jami
