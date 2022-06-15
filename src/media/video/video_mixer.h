/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
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
#include "media_stream.h"

#include <list>
#include <chrono>
#include <memory>
#include <shared_mutex>

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
    std::string id;
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

    void setActiveStream(Observable<std::shared_ptr<MediaFrame>>* ob);
    void setActiveStream(const std::string& id);
    void resetActiveStream()
    {
        activeStream_ = {};
        activeSource_ = {};
        updateLayout();
    }
    void addActiveHost();

    // TODO group, we can only use a set of string to identify actives
    bool verifyActive(const std::string& id)
    {
        return activeStream_ == id;
    }

    bool verifyActive(Observable<std::shared_ptr<MediaFrame>>* ob)
    {
        return activeSource_ == ob;
    }

    void setVideoLayout(Layout newLayout)
    {
        currentLayout_ = newLayout;
        if (currentLayout_ == Layout::GRID)
            resetActiveStream();
        layoutUpdated_ += 1;
    }

    Layout getVideoLayout() const { return currentLayout_; }

    void setOnSourcesUpdated(OnSourcesUpdatedCb&& cb) { onSourcesUpdated_ = std::move(cb); }

    MediaStream getStream(const std::string& name) const;

    std::shared_ptr<VideoFrameActiveWriter>& getVideoLocal() { return videoLocal_; }

    void updateLayout();

    std::shared_ptr<SinkClient>& getSink() { return sink_; }

    void addAudioOnlySource(const std::string& id)
    {
        std::lock_guard<std::mutex> lk(audioOnlySourcesMtx_);
        audioOnlySources_.insert(id);
        updateLayout();
    }

    void removeAudioOnlySource(const std::string& id)
    {
        std::lock_guard<std::mutex> lk(audioOnlySourcesMtx_);
        if (audioOnlySources_.erase(id))
            updateLayout();
    }

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
    std::shared_mutex rwMutex_;

    std::shared_ptr<SinkClient> sink_;

    std::chrono::time_point<std::chrono::steady_clock> nextProcess_;
    std::shared_ptr<VideoFrameActiveWriter> videoLocal_;
    std::shared_ptr<VideoFrameActiveWriter> videoLocalSecondary_;

    VideoScaler scaler_;

    ThreadLoop loop_; // as to be last member

    Layout currentLayout_ {Layout::GRID};
    Observable<std::shared_ptr<MediaFrame>>* activeSource_ {nullptr};
    std::list<std::unique_ptr<VideoMixerSource>> sources_;

    std::mutex audioOnlySourcesMtx_;
    std::set<std::string> audioOnlySources_;
    std::string activeStream_ {};

    std::atomic_int layoutUpdated_ {0};
    OnSourcesUpdatedCb onSourcesUpdated_ {};

    int64_t startTime_;
    int64_t lastTimestamp_;
};

} // namespace video
} // namespace jami
