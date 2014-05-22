/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
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

#include "libav_deps.h"
#include "video_mixer.h"
#include "logger.h"
#include "client/videomanager.h"
#include "manager.h"
#include "logger.h"

#include <cmath>

static const double FRAME_DURATION = 1/30.;

namespace sfl_video {

VideoMixer::VideoMixer(const std::string &id) :
    VideoGenerator::VideoGenerator()
    , id_(id)
    , width_(0)
    , height_(0)
    , sources_()
    , sink_(id)
    , loop_([]{return true;}, std::bind(&VideoMixer::process, this), []{})
{
    auto videoCtrl = Manager::instance().getVideoManager();
    videoCtrl->startCamera();

    // Local video camera is always attached
    if (auto shared = videoCtrl->getVideoCamera().lock())
        shared->attach(this);

    loop_.start();
}

VideoMixer::~VideoMixer()
{
    auto videoCtrl = Manager::instance().getVideoManager();
    if (auto shared = videoCtrl->getVideoCamera().lock())
        shared->detach(this);
    stop_sink();

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
            x->frameShrPtr = &frame_p;
            x->dirty = true;
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

    {
        auto lock(rwMutex_.read());

        int i = 0;
        for (const auto& x : sources_) {
            if (!loop_.isRunning())
                break;
            if (x->dirty) {
                // increment VideoFrame refcnt during rendering
                std::shared_ptr<VideoFrame> shared = *x->frameShrPtr;
                render_frame(shared.get(), i);
                x->dirty = false;
            }
            i++;
        }
    }
}

void VideoMixer::render_frame(VideoFrame* input, const int index)
{
    if (!input or !width_ or !height_)
        return;

    assert(input->get()); // design check

    VideoFrame &output = getNewFrame();
    if (!output.allocBuffer(width_, height_, VIDEO_PIXFMT_YUV420P)) {
        ERROR("VideoFrame::allocBuffer() failed");
        return;
    }

    std::shared_ptr<VideoFrame> previous_p(obtainLastFrame());
    if (previous_p) {
        previous_p->copy(output);
        previous_p.reset();
    }

    const int n = sources_.size();
    const int zoom = ceil(sqrt(n));
    const int cell_width = width_ / zoom;
    const int cell_height = height_ / zoom;
    const int xoff = (index % zoom) * cell_width;
    const int yoff = (index / zoom) * cell_height;

    VideoScaler scaler;
    scaler.scale_and_pad(*input, output, xoff, yoff, cell_width, cell_height);

    publishFrame();
}

void VideoMixer::setDimensions(int width, int height)
{
    auto lock(rwMutex_.write());

    width_ = width;
    height_ = height;

    // cleanup the previous frame to have a nice copy in rendering method
    std::shared_ptr<VideoFrame> previous_p(obtainLastFrame());
    if (previous_p)
        previous_p->clear();

    stop_sink();
    start_sink();
}

void VideoMixer::start_sink()
{
    if (sink_.start()) {
        if (this->attach(&sink_)) {
            Manager::instance().getVideoManager()->startedDecoding(id_, sink_.openedName(), width_, height_, true);
            DEBUG("MX: shm sink <%s> started: size = %dx%d",
                  sink_.openedName().c_str(), width_, height_);
        }
    } else
        WARN("MX: sink startup failed");
}

void VideoMixer::stop_sink()
{
    if (this->detach(&sink_)) {
        Manager::instance().getVideoManager()->stoppedDecoding(id_, sink_.openedName(), true);
        sink_.stop();
    }
}

int VideoMixer::getWidth() const
{ return width_; }

int VideoMixer::getHeight() const
{ return height_; }

int VideoMixer::getPixelFormat() const
{ return VIDEO_PIXFMT_YUV420P; }

} // end namespace sfl_video
