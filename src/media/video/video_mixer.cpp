/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
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

extern "C" {
#include <libavutil/display.h>
}

namespace jami {
namespace video {

struct VideoMixer::VideoMixerSource
{
    Observable<std::shared_ptr<MediaFrame>>* source {nullptr};
    int rotation {0};
    std::unique_ptr<MediaFilter> rotationFilter {nullptr};
    std::unique_ptr<VideoFrame> update_frame;
    std::unique_ptr<VideoFrame> render_frame;
    void atomic_swap_render(std::unique_ptr<VideoFrame>& other)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        render_frame.swap(other);
    }

    // Current render informations
    int x {};
    int y {};
    int w {};
    int h {};

private:
    std::mutex mutex_;
};

static constexpr const auto FRAME_DURATION = std::chrono::duration<double>(1 / 30.);

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
}

VideoMixer::~VideoMixer()
{
    stop_sink();

    if (videoLocal_) {
        videoLocal_->detach(this);
        // prefer to release it now than after the next join
        videoLocal_.reset();
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

    // Re-attach videoInput to mixer
    if (videoLocal_) {
        if (auto localInput = std::dynamic_pointer_cast<VideoInput>(videoLocal_)) {
            localInput->switchInput(input);
        }
        videoLocal_->attach(this);
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
VideoMixer::setActiveParticipant(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    activeSource_ = ob;
    layoutUpdated_ += 1;
}

void
VideoMixer::attached(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    auto lock(rwMutex_.write());

    auto src    = std::unique_ptr<VideoMixerSource>(new VideoMixerSource);
    src->source = ob;
    sources_.emplace_back(std::move(src));
    layoutUpdated_ += 1;
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
                activeSource_  = nullptr;
            }
            sources_.remove(x);
            layoutUpdated_ += 1;
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
            if (!x->update_frame)
                x->update_frame.reset(new VideoFrame);
            else
                x->update_frame->reset();
            x->update_frame->copyFrom(*std::static_pointer_cast<VideoFrame>(
                frame_p)); // copy frame content, it will be destroyed after return
            x->atomic_swap_render(x->update_frame);
            return;
        }
    }
}

void
VideoMixer::process()
{
    const auto now   = std::chrono::system_clock::now();
    const auto diff  = now - lastProcess_;
    const auto delay = FRAME_DURATION - diff;
    if (delay.count() > 0)
        std::this_thread::sleep_for(delay);
    lastProcess_ = now;

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

        int i                     = 0;
        bool activeFound          = false;
        bool needsUpdate          = layoutUpdated_ > 0;
        bool successfullyRendered = true;
        for (auto& x : sources_) {
            /* thread stop pending? */
            if (!loop_.isRunning())
                return;

            if (currentLayout_ != Layout::ONE_BIG or activeSource_ == x->source
                or (not activeSource_
                    and not activeFound) /* By default ONE_BIG will show the first source */) {
                // make rendered frame temporarily unavailable for update()
                // to avoid concurrent access.
                std::unique_ptr<VideoFrame> input;
                x->atomic_swap_render(input);

                auto wantedIndex = i;
                if (currentLayout_ == Layout::ONE_BIG) {
                    wantedIndex = 0;
                    activeFound = true;
                } else if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL) {
                    if (!activeSource_ && i == 0) {
                        activeFound = true;
                    }
                    if (activeSource_ == x->source) {
                        wantedIndex = 0;
                        activeFound = true;
                    } else if (not activeFound) {
                        wantedIndex += 1;
                    }
                }

                if (input)
                    successfullyRendered &= render_frame(output, *input, x, wantedIndex, needsUpdate);
                else
                    successfullyRendered = false;

                x->atomic_swap_render(input);
            } else if (needsUpdate) {
                x->x = 0;
                x->y = 0;
                x->w = 0;
                x->h = 0;
            }

            ++i;
        }
        if (needsUpdate and successfullyRendered) {
            layoutUpdated_ -= 1;
            if (layoutUpdated_ == 0) {
                std::vector<SourceInfo> sourcesInfo;
                sourcesInfo.reserve(sources_.size());
                for (auto& x : sources_) {
                    sourcesInfo.emplace_back(SourceInfo {x->source, x->x, x->y, x->w, x->h});
                }
                if (onSourcesUpdated_)
                    (onSourcesUpdated_)(std::move(sourcesInfo));
            }
        }
    }

    publishFrame();
}

bool
VideoMixer::render_frame(VideoFrame& output,
                         const VideoFrame& input,
                         std::unique_ptr<VideoMixerSource>& source,
                         int index,
                         bool needsUpdate)
{
    if (!width_ or !height_ or !input.pointer() or input.pointer()->format == -1)
        return false;

#ifdef RING_ACCEL
    std::shared_ptr<VideoFrame> frame {HardwareAccel::transferToMainMemory(input, AV_PIX_FMT_NV12)};
#else
    std::shared_ptr<VideoFrame> frame = input;
#endif

    int cell_width, cell_height, xoff, yoff;
    if (not needsUpdate) {
        cell_width  = source->w;
        cell_height = source->h;
        xoff        = source->x;
        yoff        = source->y;
    } else {
        const int n    = currentLayout_ == Layout::ONE_BIG ? 1 : sources_.size();
        const int zoom = currentLayout_ == Layout::ONE_BIG_WITH_SMALL ? std::max(6, n)
                                                                      : ceil(sqrt(n));
        if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL && index == 0) {
            // In ONE_BIG_WITH_SMALL, the first line at the top is the previews
            // The rest is the active source
            cell_width  = width_;
            cell_height = height_ - height_ / zoom;
        } else {
            cell_width  = width_ / zoom;
            cell_height = height_ / zoom;
        }
        if (currentLayout_ == Layout::ONE_BIG_WITH_SMALL) {
            if (index == 0) {
                xoff = 0;
                yoff = height_ / zoom; // First line height
            } else {
                xoff = (index - 1) * cell_width;
                // Show sources in center
                xoff += (width_ - (n - 1) * cell_width) / 2;
                yoff = 0;
            }
        } else {
            xoff = (index % zoom) * cell_width;
            yoff = (index / zoom) * cell_height;
        }

        // Update source's cache
        source->w = cell_width;
        source->h = cell_height;
        source->x = xoff;
        source->y = yoff;
    }

    AVFrameSideData* sideData = av_frame_get_side_data(frame->pointer(),
                                                       AV_FRAME_DATA_DISPLAYMATRIX);
    int angle                 = 0;
    if (sideData) {
        auto matrixRotation = reinterpret_cast<int32_t*>(sideData->data);
        angle               = -av_display_rotation_get(matrixRotation);
    }
    const constexpr char filterIn[] = "mixin";
    if (angle != source->rotation) {
        source->rotationFilter = video::getTransposeFilter(angle,
                                                           filterIn,
                                                           frame->width(),
                                                           frame->height(),
                                                           frame->format(),
                                                           false);
        source->rotation       = angle;
    }
    if (source->rotationFilter) {
        source->rotationFilter->feedInput(frame->pointer(), filterIn);
        frame = std::static_pointer_cast<VideoFrame>(
            std::shared_ptr<MediaFrame>(source->rotationFilter->readOutput()));
    }

    scaler_.scale_and_pad(*frame, output, xoff, yoff, cell_width, cell_height, true);
    return true;
}

void
VideoMixer::setParameters(int width, int height, AVPixelFormat format)
{
    auto lock(rwMutex_.write());

    width_  = width;
    height_ = height;
    format_ = format;

    // cleanup the previous frame to have a nice copy in rendering method
    std::shared_ptr<VideoFrame> previous_p(obtainLastFrame());
    if (previous_p)
        libav_utils::fillWithBlack(previous_p->pointer());

    start_sink();
    layoutUpdated_ += 1;
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

} // namespace video
} // namespace jami
