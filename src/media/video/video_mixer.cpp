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

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "video_mixer.h"
#include "media_buffer.h"
#include "client/videomanager.h"
#include "manager.h"
#include "media_filter.h"
#include "sinkclient.h"
#include "logger.h"
#include "filter_transpose.h"
#ifdef RING_ACCEL
#include "accel.h"
#endif

#include <cmath>
#include <unistd.h>

#include <opendht/thread_pool.h>

static constexpr auto MIN_LINE_ZOOM
    = 6; // Used by the ONE_BIG_WITH_SMALL layout for the small previews

namespace jami {
namespace video {

struct VideoMixer::VideoMixerSource
{
    Observable<std::shared_ptr<MediaFrame>>* source {nullptr};
    int rotation {0};
    std::unique_ptr<MediaFilter> rotationFilter {nullptr};
    std::shared_ptr<VideoFrame> render_frame;
    void atomic_copy(const VideoFrame& other)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto newFrame = std::make_shared<VideoFrame>();
        newFrame->copyFrom(other);
        render_frame = newFrame;
    }

    std::shared_ptr<VideoFrame> getRenderFrame()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return render_frame;
    }

    // Current render informations
    int x {};
    int y {};
    int w {};
    int h {};
    bool hasVideo {true};

private:
    std::mutex mutex_;
};

static constexpr const auto MIXER_FRAMERATE = 30;
static constexpr const auto FRAME_DURATION = std::chrono::duration<double>(1. / MIXER_FRAMERATE);

VideoMixer::VideoMixer(const std::string& id)
    : VideoGenerator::VideoGenerator()
    , id_(id)
    , sink_(Manager::instance().createSinkClient(id, true))
    , loop_([] { return true; }, std::bind(&VideoMixer::process, this), [] {})
{
    // Local video camera is the main participant
    videoLocal_ = getVideoCamera();
    if (videoLocal_)
        videoLocal_->attach(this);
    loop_.start();
    nextProcess_ = std::chrono::steady_clock::now();
}

VideoMixer::~VideoMixer()
{
    stop_sink();

    if (videoLocal_) {
        videoLocal_->detach(this);
        // prefer to release it now than after the next join
        videoLocal_.reset();
    }
    if (videoLocalSecondary_) {
        videoLocalSecondary_->detach(this);
        // prefer to release it now than after the next join
        videoLocalSecondary_.reset();
    }

    loop_.join();
}

void
VideoMixer::switchInput(const std::string& input)
{
    if (auto local = videoLocal_) {
        // Detach videoInput from mixer
        local->detach(this);
#if !VIDEO_CLIENT_INPUT
        if (auto localInput = std::dynamic_pointer_cast<VideoInput>(local)) {
            // Stop old VideoInput
            localInput->stopInput();
        }
#endif
    } else {
        videoLocal_ = getVideoCamera();
    }

    if (input.empty()) {
        JAMI_DBG("Input is empty, don't add it in the mixer");
        return;
    }

    // Re-attach videoInput to mixer
    if (videoLocal_) {
        if (auto localInput = std::dynamic_pointer_cast<VideoInput>(videoLocal_)) {
            localInput->switchInput(input);
        }
        videoLocal_->attach(this);
    }
}

void
VideoMixer::switchSecondaryInput(const std::string& input)
{
    if (auto local = videoLocalSecondary_) {
        // Detach videoInput from mixer
        local->detach(this);
#if !VIDEO_CLIENT_INPUT
        if (auto localInput = std::dynamic_pointer_cast<VideoInput>(local)) {
            // Stop old VideoInput
            localInput->stopInput();
        }
#endif
    }
    videoLocalSecondary_ = getVideoInput(input);

    if (input.empty()) {
        JAMI_DBG("Input is empty, don't add it in the mixer");
        return;
    }

    // Re-attach videoInput to mixer
    if (videoLocalSecondary_) {
        if (auto videoInput = std::dynamic_pointer_cast<VideoInput>(videoLocalSecondary_))
            videoInput->switchInput(input);
        videoLocalSecondary_->attach(this);
    }
}

void
VideoMixer::stopInput()
{
    if (auto local = std::move(videoLocal_)) {
        local->detach(this);
    }
}

void
VideoMixer::setActiveHost()
{
    activeSource_ = videoLocalSecondary_ ? videoLocalSecondary_.get() : videoLocal_.get();
    updateLayout();
}

void
VideoMixer::setActiveParticipant(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    activeSource_ = ob;
    updateLayout();
}

void
VideoMixer::updateLayout()
{
    layoutUpdated_ += 1;
}

void
VideoMixer::attached(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    auto lock(rwMutex_.write());

    auto src = std::unique_ptr<VideoMixerSource>(new VideoMixerSource);
    src->render_frame = std::make_shared<VideoFrame>();
    src->source = ob;
    sources_.emplace_back(std::move(src));
    updateLayout();
}

void
VideoMixer::detached(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    auto lock(rwMutex_.write());

    for (const auto& x : sources_) {
        if (x->source == ob) {
            // Handle the case where the current shown source leave the conference
            if (activeSource_ == ob) {
                currentLayout_ = Layout::GRID;
                activeSource_ = videoLocalSecondary_ ? videoLocalSecondary_.get()
                                                     : videoLocal_.get();
            }
            sources_.remove(x);
            updateLayout();
            break;
        }
    }
}

void
VideoMixer::update(Observable<std::shared_ptr<MediaFrame>>* ob,
                   const std::shared_ptr<MediaFrame>& frame_p)
{
    auto lock(rwMutex_.read());

    for (const auto& x : sources_) {
        if (x->source == ob) {
#ifdef RING_ACCEL
            std::shared_ptr<VideoFrame> frame;
            try {
                frame = HardwareAccel::transferToMainMemory(*std::static_pointer_cast<VideoFrame>(
                                                                frame_p),
                                                            AV_PIX_FMT_NV12);
                x->atomic_copy(*std::static_pointer_cast<VideoFrame>(frame));
            } catch (const std::runtime_error& e) {
                JAMI_ERR("Accel failure: %s", e.what());
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

    VideoFrame& output = getNewFrame();
    try {
        output.reserve(format_, width_, height_);
    } catch (const std::bad_alloc& e) {
        JAMI_ERR("VideoFrame::allocBuffer() failed");
        return;
    }

    libav_utils::fillWithBlack(output.pointer());

    {
        auto lock(rwMutex_.read());

        int i = 0;
        bool activeFound = false;
        bool needsUpdate = layoutUpdated_ > 0;
        bool successfullyRendered = false;
        for (auto& x : sources_) {
            /* thread stop pending? */
            if (!loop_.isRunning())
                return;

            if (currentLayout_ != Layout::ONE_BIG or activeSource_ == x->source) {
                // make rendered frame temporarily unavailable for update()
                // to avoid concurrent access.
                std::shared_ptr<VideoFrame> input = x->getRenderFrame();
                std::shared_ptr<VideoFrame> fooInput = std::make_shared<VideoFrame>();

                auto wantedIndex = i;
                if (currentLayout_ == Layout::ONE_BIG) {
                    wantedIndex = 0;
                    activeFound = true;
                } else if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL) {
                    if (activeSource_ == x->source) {
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
                    fooInput->reserve(format_, 1280, 720);
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
                    calc_position(x, fooInput, wantedIndex);

                if (!blackFrame) {
                    if (fooInput)
                        successfullyRendered |= render_frame(output, fooInput, x);
                    else
                        JAMI_WARN("Nothing to render for %p", x->source);
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
                std::vector<SourceInfo> sourcesInfo;
                sourcesInfo.reserve(sources_.size());
                for (auto& x : sources_) {
                    sourcesInfo.emplace_back(
                        SourceInfo {x->source, x->x, x->y, x->w, x->h, x->hasVideo});
                }
                if (onSourcesUpdated_)
                    onSourcesUpdated_(std::move(sourcesInfo));
            }
        }
    }

    output.pointer()->pts = av_rescale_q_rnd(av_gettime() - startTime_,
                                             {1, AV_TIME_BASE},
                                             {1, MIXER_FRAMERATE},
                                             static_cast<AVRounding>(AV_ROUND_NEAR_INF
                                                                     | AV_ROUND_PASS_MINMAX));
    lastTimestamp_ = output.pointer()->pts;
    publishFrame();
}

bool
VideoMixer::render_frame(VideoFrame& output,
                         const std::shared_ptr<VideoFrame>& input,
                         std::unique_ptr<VideoMixerSource>& source)
{
    if (!width_ or !height_ or !input->pointer() or input->pointer()->format == -1)
        return false;

    int cell_width = source->w;
    int cell_height = source->h;
    int xoff = source->x;
    int yoff = source->y;

    int angle = input->getOrientation();
    const constexpr char filterIn[] = "mixin";
    if (angle != source->rotation) {
        source->rotationFilter = video::getTransposeFilter(angle,
                                                           filterIn,
                                                           input->width(),
                                                           input->height(),
                                                           input->format(),
                                                           false);
        source->rotation = angle;
    }
    std::shared_ptr<VideoFrame> frame;
    if (source->rotationFilter) {
        source->rotationFilter->feedInput(input->pointer(), filterIn);
        frame = std::static_pointer_cast<VideoFrame>(
            std::shared_ptr<MediaFrame>(source->rotationFilter->readOutput()));
    } else {
        frame = input;
    }

    scaler_.scale_and_pad(*frame, output, xoff, yoff, cell_width, cell_height, true);
    return true;
}

void
VideoMixer::calc_position(std::unique_ptr<VideoMixerSource>& source,
                          const std::shared_ptr<VideoFrame>& input,
                          int index)
{
    if (!width_ or !height_)
        return;

    // Compute cell size/position
    int cell_width, cell_height, cellW_off, cellH_off;
    const int n = currentLayout_ == Layout::ONE_BIG ? 1 : sources_.size();
    const int zoom = currentLayout_ == Layout::ONE_BIG_WITH_SMALL ? std::max(MIN_LINE_ZOOM, n)
                                                                  : ceil(sqrt(n));
    if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL && index == 0) {
        // In ONE_BIG_WITH_SMALL, the first line at the top is the previews
        // The rest is the active source
        cell_width = width_;
        cell_height = height_ - height_ / zoom;
    } else {
        cell_width = width_ / zoom;
        cell_height = height_ / zoom;
    }
    if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL) {
        if (index == 0) {
            cellW_off = 0;
            cellH_off = height_ / zoom; // First line height
        } else {
            cellW_off = (index - 1) * cell_width;
            // Show sources in center
            cellW_off += (width_ - (n - 1) * cell_width) / 2;
            cellH_off = 0;
        }
    } else {
        cellW_off = (index % zoom) * cell_width;
        if (currentLayout_ == Layout::GRID && n % zoom != 0 && index >= (zoom * ((n - 1) / zoom))) {
            // Last line, center participants if not full
            cellW_off += (width_ - (n % zoom) * cell_width) / 2;
        }
        cellH_off = (index / zoom) * cell_height;
    }

    // Compute frame size/position
    float zoomW, zoomH;
    int frameW, frameH, frameW_off, frameH_off;

    if (input->getOrientation() % 180) {
        // Rotated frame
        zoomW = (float) input->height() / cell_width;
        zoomH = (float) input->width() / cell_height;
        frameH = std::round(input->width() / std::max(zoomW, zoomH));
        frameW = std::round(input->height() / std::max(zoomW, zoomH));
    } else {
        zoomW = (float) input->width() / cell_width;
        zoomH = (float) input->height() / cell_height;
        frameW = std::round(input->width() / std::max(zoomW, zoomH));
        frameH = std::round(input->height() / std::max(zoomW, zoomH));
    }

    // Center the frame in the cell
    frameW_off = cellW_off + (cell_width - frameW) / 2;
    frameH_off = cellH_off + (cell_height - frameH) / 2;

    // Update source's cache
    source->w = frameW;
    source->h = frameH;
    source->x = frameW_off;
    source->y = frameH_off;
}

void
VideoMixer::setParameters(int width, int height, AVPixelFormat format)
{
    auto lock(rwMutex_.write());

    width_ = width;
    height_ = height;
    format_ = format;

    // cleanup the previous frame to have a nice copy in rendering method
    std::shared_ptr<VideoFrame> previous_p(obtainLastFrame());
    if (previous_p)
        libav_utils::fillWithBlack(previous_p->pointer());

    start_sink();
    updateLayout();
    startTime_ = av_gettime();
}

void
VideoMixer::start_sink()
{
    stop_sink();

    if (width_ == 0 or height_ == 0) {
        JAMI_WARN("MX: unable to start with zero-sized output");
        return;
    }

    if (not sink_->start()) {
        JAMI_ERR("MX: sink startup failed");
        return;
    }

    if (this->attach(sink_.get()))
        sink_->setFrameSize(width_, height_);
}

void
VideoMixer::stop_sink()
{
    this->detach(sink_.get());
    sink_->stop();
}

int
VideoMixer::getWidth() const
{
    return width_;
}

int
VideoMixer::getHeight() const
{
    return height_;
}

AVPixelFormat
VideoMixer::getPixelFormat() const
{
    return format_;
}

MediaStream
VideoMixer::getStream(const std::string& name) const
{
    MediaStream ms;
    ms.name = name;
    ms.format = format_;
    ms.isVideo = true;
    ms.height = height_;
    ms.width = width_;
    ms.frameRate = {MIXER_FRAMERATE, 1};
    ms.timeBase = {1, MIXER_FRAMERATE};
    ms.firstTimestamp = lastTimestamp_;

    return ms;
}

} // namespace video
} // namespace jami
