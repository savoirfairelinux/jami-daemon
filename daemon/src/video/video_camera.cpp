/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "video_camera.h"
#include "video_decoder.h"
#include "check.h"

#include "manager.h"
#include "client/video_controls.h"

#include <map>
#include <string>

namespace sfl_video {

using std::string;

VideoCamera::VideoCamera(const std::map<std::string, std::string> &args) :
    VideoSource::VideoSource()
    , id_("local")
    , args_(args)
    , decoder_(0)
    , sink_()
    , bufferSize_(0)
    , previewWidth_(0)
    , previewHeight_(0)
    , scaler_()
    , frame_()
    , mixer_()
{
    start();
}

VideoCamera::~VideoCamera()
{
    stop();
    join();
}

bool VideoCamera::setup()
{
    // it's a v4l device if starting with /dev/video
    static const char * const V4L_PATH = "/dev/video";

    string format_str;
    string input = args_["input"];

    decoder_ = new VideoDecoder();

    if (args_["input"].find(V4L_PATH) != std::string::npos) {
        DEBUG("Using v4l2 format");
        format_str = "video4linux2";
    }
    if (!args_["framerate"].empty())
        decoder_->setOption("framerate", args_["framerate"].c_str());
    if (!args_["video_size"].empty())
        decoder_->setOption("video_size", args_["video_size"].c_str());
    if (!args_["channel"].empty())
        decoder_->setOption("channel", args_["channel"].c_str());

    decoder_->setInterruptCallback(interruptCb, this);

    EXIT_IF_FAIL(decoder_->openInput(input, format_str) >= 0,
                 "Could not open input \"%s\"", input.c_str());

    /* Data available, finish the decoding */
    EXIT_IF_FAIL(!decoder_->setupFromVideoData(),
                 "decoder IO startup failed");

    /* Preview frame size? (defaults from decoder) */
    if (!args_["width"].empty())
        previewWidth_ = atoi(args_["width"].c_str());
    else
        previewWidth_ = decoder_->getWidth();
    if (!args_["height"].empty())
        previewHeight_ = atoi(args_["height"].c_str());
    else
        previewHeight_ = decoder_->getHeight();

    /* Previewing setup */
    EXIT_IF_FAIL(sink_.start(), "Cannot start shared memory sink");

    frame_.setGeometry(previewWidth_, previewHeight_, VIDEO_PIXFMT_BGRA);
    bufferSize_ = frame_.getSize();
    EXIT_IF_FAIL(bufferSize_ > 0, "Incorrect buffer size for decoding");

    string name = sink_.openedName();
    Manager::instance().getVideoControls()->startedDecoding(id_, name,
                                                            previewWidth_,
                                                            previewHeight_);
    DEBUG("TX: shm sink started with size %d, width %d and height %d",
          bufferSize_, previewWidth_, previewHeight_);
    return true;
}

void VideoCamera::process()
{
    if (captureFrame() and isRunning())
        renderFrame();
}

void VideoCamera::cleanup()
{
    delete decoder_;
    Manager::instance().getVideoControls()->stoppedDecoding(id_,
                                                            sink_.openedName());
}

int VideoCamera::interruptCb(void *data)
{
    VideoCamera *context = static_cast<VideoCamera*>(data);
    return not context->isRunning();
}

bool VideoCamera::captureFrame()
{
    int ret = decoder_->decode();

    if (ret <= 0) {
        if (ret < 0)
            stop();
        return false;
    }

    return true;
}

void VideoCamera::renderFrame()
{
    // we want our rendering code to be called by the shm_sink,
    // because it manages the shared memory synchronization
    sink_.render_callback(*this, bufferSize_);

    if (mixer_)
        mixer_->render();
}

// This function is called by sink
void VideoCamera::fillBuffer(void *data)
{
    frame_.setDestination(data);
    decoder_->scale(scaler_, frame_);
}

void VideoCamera::setMixer(VideoMixer* mixer)
{
    mixer_ = mixer;
}

int VideoCamera::getWidth() const { return decoder_->getWidth(); }
int VideoCamera::getHeight() const { return decoder_->getHeight(); }

std::shared_ptr<VideoFrame> VideoCamera::waitNewFrame() { return decoder_->waitNewFrame(); }
std::shared_ptr<VideoFrame> VideoCamera::obtainLastFrame() { return decoder_->obtainLastFrame(); }

} // end namspace sfl_video
