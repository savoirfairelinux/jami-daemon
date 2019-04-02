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

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "video_mixer.h"
#include "media_buffer.h"
#include "client/videomanager.h"
#include "manager.h"
#include "sinkclient.h"
#include "logger.h"
#ifdef RING_ACCEL
#include "accel.h"
#endif

#include <cmath>
#include <unistd.h>

namespace jami { namespace video {

struct VideoMixer::VideoMixerSource {
    Observable<std::shared_ptr<MediaFrame>>* source = nullptr;
    std::unique_ptr<VideoFrame> update_frame;
    std::unique_ptr<VideoFrame> render_frame;
    void atomic_swap_render(std::unique_ptr<VideoFrame>& other) {
        std::lock_guard<std::mutex> lock(mutex_);
        render_frame.swap(other);
    }
private:
    std::mutex mutex_;
};

static constexpr const auto FRAME_DURATION = std::chrono::duration<double>(1/30.);

VideoMixer::VideoMixer(const std::string& id)
    : VideoGenerator::VideoGenerator()
    , id_(id)
    , sink_ (Manager::instance().createSinkClient(id, true))
    , loop_([]{return true;},
            std::bind(&VideoMixer::process, this),
            []{})
{
    // Local video camera is the main participant
    videoLocal_ = getVideoCamera();
    if (videoLocal_) {
        DRing::switchToCamera();
        videoLocal_->attach(this);
    }
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
VideoMixer::attached(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    auto lock(rwMutex_.write());

    auto src = std::unique_ptr<VideoMixerSource>(new VideoMixerSource);
    src->source = ob;
    sources_.emplace_back(std::move(src));
}

void
VideoMixer::detached(Observable<std::shared_ptr<MediaFrame>>* ob)
{
    auto lock(rwMutex_.write());

    for (const auto& x : sources_) {
        if (x->source == ob) {
            sources_.remove(x);
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
            x->update_frame->copyFrom(*std::static_pointer_cast<VideoFrame>(frame_p)); // copy frame content, it will be destroyed after return
            x->atomic_swap_render(x->update_frame);
            return;
        }
    }
}

void
VideoMixer::process()
{
    const auto now = std::chrono::system_clock::now();
    const auto diff = now - lastProcess_;
    const auto delay = FRAME_DURATION - diff;
    if (delay.count() > 0)
        std::this_thread::sleep_for(delay);
    lastProcess_ = now;

    VideoFrame& output = getNewFrame();
    try {
        output.reserve(AV_PIX_FMT_YUYV422, width_, height_);
    } catch (const std::bad_alloc& e) {
        JAMI_ERR("VideoFrame::allocBuffer() failed");
        return;
    }

    libav_utils::fillWithBlack(output.pointer());

    {
        auto lock(rwMutex_.read());

        int i = 0;
        for (const auto& x : sources_) {
            /* thread stop pending? */
            if (!loop_.isRunning())
                return;

            // make rendered frame temporarily unavailable for update()
            // to avoid concurrent access.
            std::unique_ptr<VideoFrame> input;
            x->atomic_swap_render(input);

            if (input)
                render_frame(output, *input, i);

            x->atomic_swap_render(input);
            ++i;
        }
    }

    publishFrame();
}

void
VideoMixer::render_frame(VideoFrame& output, const VideoFrame& input, int index)
{
    if (!width_ or !height_ or !input.pointer())
        return;

#ifdef RING_ACCEL
    auto framePtr = HardwareAccel::transferToMainMemory(input, AV_PIX_FMT_NV12);
    const auto& swFrame = *framePtr;
#else
    const auto& swFrame = input;
#endif

    const int n = sources_.size();
    const int zoom = ceil(sqrt(n));
    int cell_width = width_ / zoom;
    int cell_height = height_ / zoom;
    int xoff = (index % zoom) * cell_width;
    int yoff = (index / zoom) * cell_height;

    scaler_.scale_and_pad(swFrame, output, xoff, yoff, cell_width, cell_height, true);
}

void
VideoMixer::setDimensions(int width, int height)
{
    auto lock(rwMutex_.write());

    width_ = width;
    height_ = height;

    // cleanup the previous frame to have a nice copy in rendering method
    std::shared_ptr<VideoFrame> previous_p(obtainLastFrame());
    if (previous_p)
        libav_utils::fillWithBlack(previous_p->pointer());

    start_sink();
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
{ return width_; }

int
VideoMixer::getHeight() const
{ return height_; }

AVPixelFormat
VideoMixer::getPixelFormat() const
{ return AV_PIX_FMT_YUYV422; }

}} // namespace jami::video
