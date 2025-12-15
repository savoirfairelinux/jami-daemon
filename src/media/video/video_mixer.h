/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
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

struct StreamInfo
{
    std::string callId;
    std::string streamId;
};

struct SourceInfo
{
    Observable<std::shared_ptr<MediaFrame>>* source;
    int x;
    int y;
    int w;
    int h;
    bool hasVideo;
    std::string callId;
    std::string streamId;
};
using OnSourcesUpdatedCb = std::function<void(std::vector<SourceInfo>&&)>;

enum class Layout {
    GRID,               // The participants are shown in equal-sized grid cells
    ONE_BIG_WITH_SMALL, // One active (speaking) participant is shown big, others in small previews
    ONE_BIG             // Only one active (speaking) participant is shown
};

class VideoMixer : public VideoGenerator, public VideoFramePassiveReader
{
public:
    VideoMixer(const std::string& id, const std::string& localInput = {}, bool attachHost = false);
    ~VideoMixer();

    void setParameters(int width, int height, AVPixelFormat format = AV_PIX_FMT_YUV422P);

    int getWidth() const override;
    int getHeight() const override;
    AVPixelFormat getPixelFormat() const override;

    // as VideoFramePassiveReader
    void update(Observable<std::shared_ptr<MediaFrame>>* ob, const std::shared_ptr<MediaFrame>& v) override;
    void attached(Observable<std::shared_ptr<MediaFrame>>* ob) override;
    void detached(Observable<std::shared_ptr<MediaFrame>>* ob) override;

    /**
     * Set all inputs at once
     * @param inputs        New inputs
     * @note previous inputs will be stopped
     */
    void switchInputs(const std::vector<std::string>& inputs);
    /**
     * Stop all inputs
     */
    void stopInputs();

    void setActiveStream(const std::string& id);
    void resetActiveStream()
    {
        activeStream_.clear();
        updateLayout();
    }

    bool verifyActive(const std::string& id) { return activeStream_ == id; }

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

    std::shared_ptr<VideoFrameActiveWriter> getVideoLocal() const
    {
        if (!localInputs_.empty())
            return *localInputs_.begin();
        return {};
    }

    void updateLayout();

    std::shared_ptr<SinkClient>& getSink() { return sink_; }

    void addAudioOnlySource(const std::string& callId, const std::string& streamId)
    {
        std::unique_lock lk(audioOnlySourcesMtx_);
        audioOnlySources_.insert({callId, streamId});
        lk.unlock();
        updateLayout();
    }

    void removeAudioOnlySource(const std::string& callId, const std::string& streamId)
    {
        std::unique_lock lk(audioOnlySourcesMtx_);
        if (audioOnlySources_.erase({callId, streamId})) {
            lk.unlock();
            updateLayout();
        }
    }

    void attachVideo(Observable<std::shared_ptr<MediaFrame>>* frame,
                     const std::string& callId,
                     const std::string& streamId);
    void detachVideo(Observable<std::shared_ptr<MediaFrame>>* frame);

    StreamInfo streamInfo(Observable<std::shared_ptr<MediaFrame>>* frame) const
    {
        std::lock_guard lk(videoToStreamInfoMtx_);
        auto it = videoToStreamInfo_.find(frame);
        if (it == videoToStreamInfo_.end())
            return {};
        return it->second;
    }

private:
    NON_COPYABLE(VideoMixer);
    struct VideoMixerSource;

    bool renderFrame(VideoFrame& output,
                     const std::shared_ptr<VideoFrame>& input,
                     std::unique_ptr<VideoMixerSource>& source);

    void calculatePosition(std::unique_ptr<VideoMixerSource>& source,
                           const std::shared_ptr<VideoFrame>& input,
                           int index);

    void startSink();
    void stopSink();

    void stopInput(const std::shared_ptr<VideoFrameActiveWriter>& input);

    // void setup() ?
    void process();
    // void cleanup() ?

private:
    // Important to keep this order for id_, sink_ and loop_
    // Because these attributes are initialized in the member initializer list
    const std::string id_;
    std::shared_ptr<SinkClient> sink_;
    ThreadLoop loop_;

    Layout currentLayout_ {Layout::GRID};

    // Dimensions of output frames produced by the mixer
    int frameWidth_ = 0;
    int frameHeight_ = 0;

    // Used for scaling frames
    VideoScaler scaler_;

    // Format of the pixels in the output frames
    AVPixelFormat pixelFormat_ = AV_PIX_FMT_YUV422P;

    std::shared_mutex sourcesMtx_;
    std::list<std::unique_ptr<VideoMixerSource>> sources_;

    std::mutex audioOnlySourcesMtx_;
    std::set<std::pair<std::string, std::string>> audioOnlySources_;

    std::mutex localInputsMtx_;
    std::vector<std::shared_ptr<VideoFrameActiveWriter>> localInputs_ {};

    mutable std::mutex videoToStreamInfoMtx_ {};
    // Lookup table to get callId/streamId for a frame
    std::map<Observable<std::shared_ptr<MediaFrame>>*, StreamInfo> videoToStreamInfo_ {};

    std::string activeStream_ {};

    std::atomic_int layoutUpdated_ {0};
    OnSourcesUpdatedCb onSourcesUpdated_ {};

    // Target timestamp for next frame to keep consistent FPS output
    std::chrono::time_point<std::chrono::steady_clock> nextProcess_;

    int64_t startTime_;
    int64_t lastTimestamp_;
};

} // namespace video
} // namespace jami
