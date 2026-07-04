/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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
#include <atomic>
#include <utility>
#include <vector>

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
using OnFormatChangedCb = std::function<void(int width, int height, int frameRate)>;

enum class Layout : uint8_t { GRID, ONE_BIG_WITH_SMALL, ONE_BIG };

class VideoMixer : public VideoGenerator, public VideoFramePassiveReader
{
public:
    /**
     * Format of a mixer input, as seen by the dynamic-format policy.
     */
    struct SourceSpec
    {
        int width;
        int height;
        int frameRate;
        bool active; ///< matches the mixer active stream
    };

    VideoMixer(const std::string& id, const std::string& localInput = {}, bool attachHost = false);
    ~VideoMixer();

    void setParameters(int width, int height, AVPixelFormat format = AV_PIX_FMT_YUV422P);

    /**
     * Let the mixer adapt its composition surface and frame rate to its
     * sources (shrink-to-fit), within the given caps.
     *
     * @param baseCap   Surface never exceeded by the grid layouts
     * @param bigCap    Surface allowed for active-source layouts with at most
     *                  MAX_BIG_CAP_SOURCES sources (e.g. a 4K screen share)
     * @param maxFrameRate  Upper bound for the source-following frame rate
     */
    void enableDynamicFormat(std::pair<int, int> baseCap, std::pair<int, int> bigCap, int maxFrameRate);
    void setOnFormatChanged(OnFormatChangedCb&& cb) { onFormatChanged_ = std::move(cb); }

    /**
     * Compute the composition surface for the given sources (pure policy).
     * Returns {0, 0} when no source provides a usable resolution.
     */
    static std::pair<int, int> computeTargetSurface(Layout layout,
                                                    const std::vector<SourceSpec>& sources,
                                                    std::pair<int, int> baseCap,
                                                    std::pair<int, int> bigCap);

    /**
     * Frame rate following the fastest source, in [MIXER_FRAMERATE, maxFrameRate].
     */
    static int computeTargetFrameRate(const std::vector<SourceSpec>& sources, int maxFrameRate);

    int getWidth() const override;
    int getHeight() const override;
    int getFrameRate() const;
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
        activeStream_ = {};
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

    bool render_frame(VideoFrame& output,
                      const std::shared_ptr<VideoFrame>& input,
                      std::unique_ptr<VideoMixerSource>& source);

    void calc_position(std::unique_ptr<VideoMixerSource>& source, const std::shared_ptr<VideoFrame>& input, int index);

    void startSink();
    void stopSink();

    void process();

    /**
     * Periodically evaluate the dynamic format and apply it (with hysteresis)
     * from the process loop. Returns true when the surface was resized.
     */
    void checkDynamicFormat();

    const std::string id_;
    int width_ = 0;
    int height_ = 0;
    AVPixelFormat format_ = AV_PIX_FMT_YUV422P;
    mutable std::shared_mutex rwMutex_;

    std::shared_ptr<SinkClient> sink_;

    std::chrono::time_point<std::chrono::steady_clock> nextProcess_;
    std::mutex localInputsMtx_;
    std::vector<std::shared_ptr<VideoFrameActiveWriter>> localInputs_ {};
    void stopInput(const std::shared_ptr<VideoFrameActiveWriter>& input);

    VideoScaler scaler_;

    // Composition frame rate; must be declared before loop_ (initialization
    // order) as the process loop paces itself on it.
    int frameRate_;

    ThreadLoop loop_; // as to be last member

    Layout currentLayout_ {Layout::GRID};
    std::list<std::unique_ptr<VideoMixerSource>> sources_;

    // We need to convert call to frame
    mutable std::mutex videoToStreamInfoMtx_ {};
    std::map<Observable<std::shared_ptr<MediaFrame>>*, StreamInfo> videoToStreamInfo_ {};

    std::mutex audioOnlySourcesMtx_;
    std::set<std::pair<std::string, std::string>> audioOnlySources_;
    std::string activeStream_ {};

    std::atomic_int layoutUpdated_ {0};
    OnSourcesUpdatedCb onSourcesUpdated_ {};

    // Dynamic (source-following) format state, owned by the process loop.
    std::atomic_bool dynamicFormat_ {false};
    std::pair<int, int> baseCap_ {0, 0};
    std::pair<int, int> bigCap_ {0, 0};
    int maxFrameRate_ {0};
    std::chrono::steady_clock::time_point nextFormatCheck_ {};
    std::chrono::steady_clock::time_point lastFormatChange_ {};
    OnFormatChangedCb onFormatChanged_ {};

    int64_t startTime_;
    int64_t lastTimestamp_;
};

} // namespace video
} // namespace jami
