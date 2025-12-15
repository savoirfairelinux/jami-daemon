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

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "video_mixer.h"
#include "media_buffer.h"
#include "client/videomanager.h"
#include "manager.h"
#include "media_filter.h"
#include "sinkclient.h"
#include "logger.h"
#include "filter_transpose.h"
#ifdef ENABLE_HWACCEL
#include "accel.h"
#endif
#include "connectivity/sip_utils.h"

#include <cmath>
#include <unistd.h>
#include <mutex>

#include "videomanager_interface.h"
#include <opendht/thread_pool.h>

static constexpr auto MIN_LINE_ZOOM = 6; // Used by the ONE_BIG_WITH_SMALL layout for the small previews

namespace jami {
namespace video {

struct VideoMixer::VideoMixerSource
{
    Observable<std::shared_ptr<MediaFrame>>* source {nullptr};
    int rotation {0};
    std::unique_ptr<MediaFilter> rotationFilter {nullptr};
    std::shared_ptr<VideoFrame> renderFrame;

    void atomic_copy(const VideoFrame& other)
    {
        std::lock_guard lock(renderFrameMtx_);
        auto newFrame = std::make_shared<VideoFrame>();
        newFrame->copyFrom(other);
        renderFrame = newFrame;
    }

    std::shared_ptr<VideoFrame> getRenderFrame()
    {
        std::lock_guard lock(renderFrameMtx_);
        return renderFrame;
    }

    // Current render information
    int x {};
    int y {};
    int w {};
    int h {};
    bool hasVideo {true};

private:
    std::mutex renderFrameMtx_;
};

static constexpr const auto MIXER_FRAMERATE = 30;
static constexpr const auto FRAME_DURATION = std::chrono::duration<double>(1. / MIXER_FRAMERATE);

VideoMixer::VideoMixer(const std::string& id, const std::string& localInput, bool attachHost)
    : VideoGenerator::VideoGenerator()
    , id_(id)
    , sink_(Manager::instance().createSinkClient(id, true))
    , loop_([] { return true; }, std::bind(&VideoMixer::process, this), [] {})
{
    // Local video camera is the main participant
    if (not localInput.empty() && attachHost) {
        // as of DEC 15 2025 this is dead code. will potentially be removed when purpose is cleared.
        auto videoInput = getVideoInput(localInput);
        localInputs_.emplace_back(videoInput);
        attachVideo(videoInput.get(), "", sip_utils::streamId("", sip_utils::DEFAULT_VIDEO_STREAMID));
    }
    loop_.start();
    nextProcess_ = std::chrono::steady_clock::now();

    JAMI_DEBUG("[mixer:{}] New instance created", id_);
}

VideoMixer::~VideoMixer()
{
    stopSink();
    stopInputs();

    loop_.join();

    JAMI_DEBUG("[mixer:{}] Instance destroyed", id_);
}

void
VideoMixer::switchInputs(const std::vector<std::string>& inputs)
{
    // Do not stop video inputs that are already there
    // But only detach it to get new index
    std::lock_guard lk(localInputsMtx_);
    decltype(localInputs_) newInputs;
    newInputs.reserve(inputs.size());
    for (const auto& input : inputs) {
        auto videoInput = getVideoInput(input);
        // Note, video can be a previously stopped device (eg. when restarting a screen share)
        // in this case, the videoInput will be found and must be restarted
        videoInput->restart();
        auto it = std::find(localInputs_.cbegin(), localInputs_.cend(), videoInput);
        if (it != localInputs_.cend()) {
            videoInput->detach(this);
            localInputs_.erase(it);
        }

        newInputs.emplace_back(std::move(videoInput));
    }

    // Stop other video inputs
    stopInputs();
    localInputs_ = std::move(newInputs);

    // Re-attach videoInput to mixer
    for (size_t i = 0; i < localInputs_.size(); ++i) {
        auto& input = localInputs_[i];
        attachVideo(input.get(), "", sip_utils::streamId("", fmt::format("video_{}", i)));
    }
}

void
VideoMixer::stopInput(const std::shared_ptr<VideoFrameActiveWriter>& input)
{
    // Detach videoInputs from mixer
    input->detach(this);
}

void
VideoMixer::stopInputs()
{
    for (auto& input : localInputs_)
        stopInput(input);
    localInputs_.clear();
}

void
VideoMixer::setActiveStream(const std::string& id)
{
    activeStream_ = id;
    updateLayout();
}

void
VideoMixer::updateLayout()
{
    if (activeStream_.empty())
        currentLayout_ = Layout::GRID;
    layoutUpdated_ += 1;
}

void
VideoMixer::attachVideo(Observable<std::shared_ptr<MediaFrame>>* frame,
                        const std::string& callId,
                        const std::string& streamId)
{
    if (!frame)
        return;

    JAMI_DEBUG("[mixer:{}] Attaching video with streamId {}", id_, streamId);

    {
        std::lock_guard lk(videoToStreamInfoMtx_);
        videoToStreamInfo_[frame] = StreamInfo {callId, streamId};
    }

    frame->attach(this);
}

void
VideoMixer::detachVideo(Observable<std::shared_ptr<MediaFrame>>* frame)
{
    if (!frame)
        return;

    bool shouldDetach = false;

    std::unique_lock lk(videoToStreamInfoMtx_);
    auto it = videoToStreamInfo_.find(frame);
    if (it != videoToStreamInfo_.end()) {
        JAMI_DEBUG("[mixer:{}] Detaching video of call {}", id_, it->second.callId);
        shouldDetach = true;
        // Handle the case where the currently shown source leaves the conference
        // Note: do not call resetActiveStream() to avoid multiple updates
        if (verifyActive(it->second.streamId))
            activeStream_.clear();
        videoToStreamInfo_.erase(it);
    }

    lk.unlock();
    if (shouldDetach)
        frame->detach(this);
}

void
VideoMixer::attached(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    std::unique_lock lock(sourcesMtx_);

    auto src = std::unique_ptr<VideoMixerSource>(new VideoMixerSource);
    src->renderFrame = std::make_shared<VideoFrame>();
    src->source = ob;
    JAMI_DEBUG("[mixer:{}] Adding new source [{}]", id_, fmt::ptr(src.get()));
    sources_.emplace_back(std::move(src));
    JAMI_DEBUG("[mixer:{}] Total number of sources: {}", id_, sources_.size());
    updateLayout();
}

void
VideoMixer::detached(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    std::unique_lock lock(sourcesMtx_);

    for (const auto& x : sources_) {
        if (x->source == ob) {
            JAMI_DEBUG("[mixer:{}] Removing source [{}]", id_, fmt::ptr(x.get()));
            sources_.remove(x);
            JAMI_DEBUG("[mixer:{}] Total number of sources: {}", id_, sources_.size());
            updateLayout();
            break;
        }
    }
}

void
VideoMixer::update(Observable<std::shared_ptr<MediaFrame>>* ob, const std::shared_ptr<MediaFrame>& frame_p)
{
    std::shared_lock lock(sourcesMtx_);

    for (const auto& x : sources_) {
        if (x->source == ob) {
#ifdef ENABLE_HWACCEL
            std::shared_ptr<VideoFrame> frame;
            try {
                frame = HardwareAccel::transferToMainMemory(*std::static_pointer_cast<VideoFrame>(frame_p),
                                                            AV_PIX_FMT_NV12);
                x->atomic_copy(*std::static_pointer_cast<VideoFrame>(frame));
            } catch (const std::runtime_error& e) {
                JAMI_ERROR("[mixer:{}] Accel failure: {}", id_, e.what());
                return;
            }
#else
            x->atomic_copy(*std::static_pointer_cast<VideoFrame>(frame_p));
#endif
            return;
        }
    }
}

void
VideoMixer::process()
{
    nextProcess_ += std::chrono::duration_cast<std::chrono::microseconds>(FRAME_DURATION);

    const auto delay = nextProcess_ - std::chrono::steady_clock::now();
    if (delay.count() > 0)
        std::this_thread::sleep_for(delay);

    if (frameWidth_ == 0 or frameHeight_ == 0) {
        // Nothing to do
        return;
    }

    VideoFrame& output = getNewFrame();
    try {
        output.reserve(pixelFormat_, frameWidth_, frameHeight_);
    } catch (const std::bad_alloc& e) {
        JAMI_ERROR("[mixer:{}] VideoFrame::allocBuffer() failed: {}", id_, e.what());
        return;
    }

    libav_utils::fillWithBlack(output.pointer());

    {
        std::lock_guard lk(audioOnlySourcesMtx_);
        std::shared_lock lock(sourcesMtx_);

        int i = 0;
        bool activeFound = false;
        bool needsUpdate = layoutUpdated_ > 0;
        bool successfullyRendered = audioOnlySources_.size() != 0 && sources_.size() == 0;
        std::vector<SourceInfo> sourcesInfo;
        sourcesInfo.reserve(sources_.size() + audioOnlySources_.size());
        // add all audioonlysources
        for (auto& [callId, streamId] : audioOnlySources_) {
            auto active = verifyActive(streamId);
            if (currentLayout_ != Layout::ONE_BIG or active) {
                sourcesInfo.emplace_back(SourceInfo {{}, 0, 0, 10, 10, false, callId, streamId});
            }
            if (currentLayout_ == Layout::ONE_BIG) {
                if (active)
                    successfullyRendered = true;
                else
                    sourcesInfo.emplace_back(SourceInfo {{}, 0, 0, 0, 0, false, callId, streamId});
                // Add all participants info even in ONE_BIG layout.
                // The width and height set to 0 here will led the peer to filter them out.
            }
        }
        // add video sources
        for (auto& x : sources_) {
            /* thread stop pending? */
            if (!loop_.isRunning())
                return;

            auto sinfo = streamInfo(x->source);
            auto activeSource = verifyActive(sinfo.streamId);
            if (currentLayout_ != Layout::ONE_BIG or activeSource) {
                // make rendered frame temporarily unavailable for update()
                // to avoid concurrent access.
                std::shared_ptr<VideoFrame> input = x->getRenderFrame();
                std::shared_ptr<VideoFrame> fooInput = std::make_shared<VideoFrame>();

                auto wantedIndex = i;
                if (currentLayout_ == Layout::ONE_BIG) {
                    wantedIndex = 0;
                    activeFound = true;
                } else if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL) {
                    if (activeSource) {
                        wantedIndex = 0;
                        activeFound = true;
                    } else if (not activeFound) {
                        wantedIndex += 1;
                    }
                }

                auto hasVideo = x->hasVideo;
                bool blackFrame = false;

                if (!input->height() or !input->width()) {
                    successfullyRendered = true;
                    fooInput->reserve(pixelFormat_, frameWidth_, frameHeight_);
                    blackFrame = true;
                } else {
                    fooInput.swap(input);
                }

                // If orientation changed or if the first valid frame for source
                // is received -> trigger layout calculation and confInfo update
                if (x->rotation != fooInput->getOrientation() or !x->w or !x->h) {
                    updateLayout();
                    needsUpdate = true;
                }

                if (needsUpdate)
                    calculatePosition(x, fooInput, wantedIndex);

                if (!blackFrame) {
                    if (fooInput)
                        successfullyRendered |= renderFrame(output, fooInput, x);
                    else
                        JAMI_WARNING("[mixer:{}] Nothing to render for source [{}]", id_, fmt::ptr(x->source));
                }

                x->hasVideo = !blackFrame && successfullyRendered;
                if (hasVideo != x->hasVideo) {
                    updateLayout();
                    needsUpdate = true;
                }

            } else if (needsUpdate) {
                x->x = 0;
                x->y = 0;
                x->w = 0;
                x->h = 0;
                x->hasVideo = false;
            }

            ++i;
        }
        if (needsUpdate and successfullyRendered) {
            layoutUpdated_ -= 1;
            if (layoutUpdated_ == 0) {
                for (auto& x : sources_) {
                    auto sinfo = streamInfo(x->source);
                    sourcesInfo.emplace_back(
                        SourceInfo {x->source, x->x, x->y, x->w, x->h, x->hasVideo, sinfo.callId, sinfo.streamId});
                }
                if (onSourcesUpdated_)
                    onSourcesUpdated_(std::move(sourcesInfo));
            }
        }
    }

    output.pointer()->pts = av_rescale_q_rnd(av_gettime() - startTime_,
                                             {1, AV_TIME_BASE},
                                             {1, MIXER_FRAMERATE},
                                             static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    lastTimestamp_ = output.pointer()->pts;
    publishFrame();
}

bool
VideoMixer::renderFrame(VideoFrame& output,
                        const std::shared_ptr<VideoFrame>& input,
                        std::unique_ptr<VideoMixerSource>& source)
{
    if (!frameWidth_ or !frameHeight_ or !input->pointer() or input->pointer()->format == -1)
        return false;

    int cell_width = source->w;
    int cell_height = source->h;
    int xoff = source->x;
    int yoff = source->y;

    int angle = input->getOrientation();
    const constexpr char filterIn[] = "mixin";
    if (angle != source->rotation) {
        source->rotationFilter
            = video::getTransposeFilter(angle, filterIn, input->width(), input->height(), input->format(), false);
        source->rotation = angle;
    }
    std::shared_ptr<VideoFrame> frame;
    if (source->rotationFilter) {
        source->rotationFilter->feedInput(input->pointer(), filterIn);
        frame = std::static_pointer_cast<VideoFrame>(std::shared_ptr<MediaFrame>(source->rotationFilter->readOutput()));
    } else {
        frame = input;
    }

    scaler_.scale_and_pad(*frame, output, xoff, yoff, cell_width, cell_height, true);
    return true;
}

void
VideoMixer::calculatePosition(std::unique_ptr<VideoMixerSource>& source,
                              const std::shared_ptr<VideoFrame>& input,
                              int index)
{
    if (!frameWidth_ or !frameHeight_)
        return;

    // Compute cell size/position
    int cellWidth, cellHeight, cellWidthOffset, cellHeightOffset;

    // This function is only called by process(), which holds sourcesMtx_ :)
    const int n = currentLayout_ == Layout::ONE_BIG ? 1 : sources_.size();

    const int zoom = currentLayout_ == Layout::ONE_BIG_WITH_SMALL ? std::max(MIN_LINE_ZOOM, n) : ceil(sqrt(n));
    if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL && index == 0) {
        // In ONE_BIG_WITH_SMALL, the first line at the top is the previews
        // The rest is the active source
        cellWidth = frameWidth_;
        cellHeight = frameHeight_ - frameHeight_ / zoom;
    } else {
        cellWidth = frameWidth_ / zoom;
        cellHeight = frameHeight_ / zoom;

        if (n == 1) {
            // On some platforms (at least macOS/android) - Having one frame at the same
            // size of the mixer cause it to be grey.
            // Removing some pixels solve this. We use 16 because it's a multiple of 8
            // (value that we prefer for video management)
            cellWidth -= 16;
            cellHeight -= 16;
        }
    }
    if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL) {
        if (index == 0) {
            cellWidthOffset = 0;
            cellHeightOffset = frameHeight_ / zoom; // First line height
        } else {
            cellWidthOffset = (index - 1) * cellWidth;
            // Show sources in center
            cellWidthOffset += (frameWidth_ - (n - 1) * cellWidth) / 2;
            cellHeightOffset = 0;
        }
    } else {
        cellWidthOffset = (index % zoom) * cellWidth;
        if (currentLayout_ == Layout::GRID && n % zoom != 0 && index >= (zoom * ((n - 1) / zoom))) {
            // Last line, center participants if not full
            cellWidthOffset += (frameWidth_ - (n % zoom) * cellWidth) / 2;
        }
        cellHeightOffset = (index / zoom) * cellHeight;
        if (n == 1) {
            // Centerize (cellwidth = width_ - 16)
            cellWidthOffset += 8;
            cellHeightOffset += 8;
        }
    }

    // Compute frame size/position
    float zoomWidth, zoomHeight;
    int frameWidth, frameHeight, frameWidthOffset, frameHeightOffset;

    if (input->getOrientation() % 180) {
        // Rotated frame
        zoomWidth = static_cast<float>(input->height()) / static_cast<float>(cellWidth);
        zoomHeight = static_cast<float>(input->width()) / static_cast<float>(cellHeight);
        frameHeight = static_cast<int>(std::round(static_cast<float>(input->width()) / std::max(zoomWidth, zoomHeight)));
        frameWidth = static_cast<int>(std::round(static_cast<float>(input->height()) / std::max(zoomWidth, zoomHeight)));
    } else {
        zoomWidth = static_cast<float>(input->width()) / static_cast<float>(cellWidth);
        zoomHeight = static_cast<float>(input->height()) / static_cast<float>(cellHeight);
        frameWidth = static_cast<int>(std::round(static_cast<float>(input->width()) / std::max(zoomWidth, zoomHeight)));
        frameHeight = static_cast<int>(
            std::round(static_cast<float>(input->height()) / std::max(zoomWidth, zoomHeight)));
    }

    // Center the frame in the cell
    frameWidthOffset = cellWidthOffset + (cellWidth - frameWidth) / 2;
    frameHeightOffset = cellHeightOffset + (cellHeight - frameHeight) / 2;

    // Update source's cache
    source->w = frameWidth;
    source->h = frameHeight;
    source->x = frameWidthOffset;
    source->y = frameHeightOffset;
}

void
VideoMixer::setParameters(int width, int height, AVPixelFormat format)
{
    std::unique_lock lock(sourcesMtx_);

    frameWidth_ = width;
    frameHeight_ = height;
    pixelFormat_ = format;

    // cleanup the previous frame to have a nice copy in rendering method
    if (auto lastFrame = obtainLastFrame())
        libav_utils::fillWithBlack(lastFrame->pointer());

    startSink();
    updateLayout();
    startTime_ = av_gettime();
}

void
VideoMixer::startSink()
{
    stopSink();

    if (frameWidth_ == 0 or frameHeight_ == 0) {
        JAMI_WARNING("[mixer:{}] Unable to start because of invalid frame dimensions (w={}, h={})",
                     id_,
                     frameWidth_,
                     frameHeight_);
        return;
    }

    if (not sink_->start()) {
        // Seems to be dead code, sink_->start() always returns true...
        JAMI_ERROR("[mixer:{}] Sink startup failed", id_);
        return;
    }

    if (this->attach(sink_.get()))
        sink_->setFrameSize(frameWidth_, frameHeight_);
    else
        JAMI_WARNING("[mixer:{}] Could not attach sink", id_);
}

void
VideoMixer::stopSink()
{
    this->detach(sink_.get());
    sink_->stop();
}

int
VideoMixer::getWidth() const
{
    return frameWidth_;
}

int
VideoMixer::getHeight() const
{
    return frameHeight_;
}

AVPixelFormat
VideoMixer::getPixelFormat() const
{
    return pixelFormat_;
}

MediaStream
VideoMixer::getStream(const std::string& name) const
{
    MediaStream ms;
    ms.name = name;
    ms.format = pixelFormat_;
    ms.isVideo = true;
    ms.height = frameHeight_;
    ms.width = frameWidth_;
    ms.frameRate = {MIXER_FRAMERATE, 1};
    ms.timeBase = {1, MIXER_FRAMERATE};
    ms.firstTimestamp = lastTimestamp_;

    return ms;
}

} // namespace video
} // namespace jami
