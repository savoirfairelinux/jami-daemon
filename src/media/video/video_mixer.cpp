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

#include "libav_deps.h" // MUST BE INCLUDED FIRST

#include "video_mixer.h"
#include "media_buffer.h"
#include "logger.h"
#include "client/videomanager.h"
#include "manager.h"
#include "sinkclient.h"
#include "logger.h"

#include <cmath>
#include <unistd.h>

namespace ring { namespace video {

static const double FRAME_DURATION = 1/30.;

VideoMixer::VideoMixer(const std::string &id)
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

void VideoMixer::attached(Observable<std::shared_ptr<VideoFrame> >* ob)
{
    auto lock(rwMutex_.write());

    VideoMixerSource* src = new VideoMixerSource;
    src->source = ob;
    sources_.push_back(src);
}

void VideoMixer::detached(Observable<std::shared_ptr<VideoFrame> >* ob)
{
    auto lock(rwMutex_.write());

    for (auto x : sources_) {
        if (x->source == ob) {
            sources_.remove(x);
            delete x;
            break;
        }
    }
}

void VideoMixer::update(Observable<std::shared_ptr<VideoFrame> >* ob,
                        std::shared_ptr<VideoFrame>& frame_p)
{
    auto lock(rwMutex_.read());

    for (const auto& x : sources_) {
        if (x->source == ob) {
            if (!x->update_frame)
                x->update_frame.reset(new VideoFrame);
            *x->update_frame = *frame_p;
            x->atomic_swap_render(x->update_frame);
            return;
        }
    }
}

void VideoMixer::process()
{
    const auto now = std::chrono::system_clock::now();
    const std::chrono::duration<double> diff = now - lastProcess_;
    const double delay = FRAME_DURATION - diff.count();
    if (delay > 0)
        usleep(delay * 1e6);
    lastProcess_ = now;

    VideoFrame& output = getNewFrame();
    try {
        output.reserve(VIDEO_PIXFMT_YUV420P, width_, height_);
    } catch (const std::bad_alloc& e) {
        RING_ERR("VideoFrame::allocBuffer() failed");
        return;
    }

    yuv422_clear_to_black(output);

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

    const int n = sources_.size();
    const int zoom = ceil(sqrt(n));
    int cell_width = width_ / zoom;
    int cell_height = height_ / zoom;
    int xoff = (index % zoom) * cell_width;
    int yoff = (index / zoom) * cell_height;

    scaler_.scale_and_pad(input, output, xoff, yoff, cell_width, cell_height, true);
}

void VideoMixer::setDimensions(int width, int height)
{
    auto lock(rwMutex_.write());

    width_ = width;
    height_ = height;

    // cleanup the previous frame to have a nice copy in rendering method
    std::shared_ptr<VideoFrame> previous_p(obtainLastFrame());
    if (previous_p)
        yuv422_clear_to_black(*previous_p);

    start_sink();
}

void
VideoMixer::start_sink()
{
    stop_sink();

    if (not sink_->start()) {
        RING_ERR("MX: sink startup failed");
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

int VideoMixer::getWidth() const
{ return width_; }

int VideoMixer::getHeight() const
{ return height_; }

int VideoMixer::getPixelFormat() const
{ return VIDEO_PIXFMT_YUV420P; }

}} // namespace ring::video
