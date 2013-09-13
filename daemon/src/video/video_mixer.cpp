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
#include "check.h"

#include <cmath>

namespace sfl_video {

VideoMixer::VideoMixer() :
    VideoGenerator::VideoGenerator()
    , sourceScaler_()
    , scaledFrame_()
    , width_(0)
    , height_(0)
    , renderMutex_()
    , renderCv_()
{
    start();
}

VideoMixer::~VideoMixer()
{
    stop();
    join();
}

void VideoMixer::process()
{
    waitForUpdate();
    rendering();
}

void VideoMixer::waitForUpdate()
{
    std::unique_lock<std::mutex> lk(renderMutex_);
    renderCv_.wait(lk);
}

void VideoMixer::update(Observable<VideoFrameSP>* ob, VideoFrameSP& frame_p)
{ renderCv_.notify_one(); }

void VideoMixer::rendering()
{
    if (!width_ or !height_)
        return;

#if 0
    // For all sources:
    //   - take source frame
    //   - scale it down and layout it
    //   - publish the result frame
    // Current layout is a squared distribution

    const int n=sourceList_.size();
    const int zoom=ceil(sqrt(n));
    const int cell_width=width_ / zoom;
    const int cell_height=height_ / zoom;

    VideoFrame &output = getNewFrame();

    // Blit frame function support only YUV420P pixel format
    if (!output.allocBuffer(width_, height_, VIDEO_PIXFMT_YUV420P))
        WARN("VideoFrame::allocBuffer() failed");

    if (!scaledFrame_.allocBuffer(cell_width, cell_height, VIDEO_PIXFMT_YUV420P))
        WARN("VideoFrame::allocBuffer() failed");

    int lastInputWidth=0;
    int lastInputHeight=0;
    int i=0;
    for (VideoNode* src : sourceList_) {
        int xoff = (i % zoom) * cell_width;
        int yoff = (i / zoom) * cell_height;

        VideoFrameSP input=src->obtainLastFrame();
        if (input) {
            // scaling context allocation may be time consuming
            // so reset it only if needed
            if (input->getWidth() != lastInputWidth ||
                input->getHeight() != lastInputHeight)
                sourceScaler_.reset();

            sourceScaler_.scale(*input, scaledFrame_);
            output.blit(scaledFrame_, xoff, yoff);

            lastInputWidth = input->getWidth();
            lastInputHeight = input->getHeight();
        }

        i++;
    }
    publishFrame();
#endif
}

void VideoMixer::setDimensions(int width, int height)
{
    // FIXME: unprotected write (see rendering())
    width_ = width;
    height_ = height;
}

int VideoMixer::getWidth() const { return width_; }
int VideoMixer::getHeight() const { return height_; }
int VideoMixer::getPixelFormat() const { return VIDEO_PIXFMT_YUV420P; }

} // end namespace sfl_video
