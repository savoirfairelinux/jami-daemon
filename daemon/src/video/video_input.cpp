/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
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

#include "video_input.h"
#include "video_decoder.h"
#include "check.h"

#include "manager.h"
#include "client/video_controls.h"

#include <map>
#include <string>

#define SINK_ID "local"

namespace sfl_video {

using std::string;

VideoInput::VideoInput(const std::map<std::string, std::string> &args) :
    VideoGenerator::VideoGenerator()
    , id_(SINK_ID)
    , args_(args)
    , decoder_(0)
    , sink_()
    , sinkWidth_(0)
    , sinkHeight_(0)
{ start(); }

VideoInput::~VideoInput()
{
    stop();
    join();
}

bool VideoInput::setup()
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
        sinkWidth_ = atoi(args_["width"].c_str());
    else
        sinkWidth_ = decoder_->getWidth();
    if (!args_["height"].empty())
        sinkHeight_ = atoi(args_["height"].c_str());
    else
        sinkHeight_ = decoder_->getHeight();

    /* Sink setup */
    EXIT_IF_FAIL(sink_.start(), "Cannot start shared memory sink");
    if (attach(&sink_)) {
        Manager::instance().getVideoControls()->startedDecoding(id_, sink_.openedName(), sinkWidth_, sinkHeight_);
        DEBUG("LOCAL: shm sink <%s> started: size = %dx%d",
              sink_.openedName().c_str(), sinkWidth_, sinkHeight_);
    }

    return true;
}

void VideoInput::process()
{ captureFrame(); }

void VideoInput::cleanup()
{
    if (detach(&sink_)) {
        Manager::instance().getVideoControls()->stoppedDecoding(id_, sink_.openedName());
        sink_.stop();
    }

    delete decoder_;
}

int VideoInput::interruptCb(void *data)
{
    VideoInput *context = static_cast<VideoInput*>(data);
    return not context->isRunning();
}

bool VideoInput::captureFrame()
{
    VideoFrame& frame = getNewFrame();
    int ret = decoder_->decode(frame);

    if (ret <= 0) {
        if (ret < 0)
            stop();
        return false;
    }

    frame.mirror();
    publishFrame();
    return true;
}

int VideoInput::getWidth() const
{ return decoder_->getWidth(); }

int VideoInput::getHeight() const
{ return decoder_->getHeight(); }

int VideoInput::getPixelFormat() const
{ return decoder_->getPixelFormat(); }



} // end namespace sfl_video
