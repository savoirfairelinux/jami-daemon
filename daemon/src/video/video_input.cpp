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
#include "client/videomanager.h"

#include <map>
#include <string>

#define SINK_ID "local"

namespace sfl_video {

static std::string
extract(const std::map<std::string, std::string>& map,
        const std::string& key)
{
    const auto iter = map.find(key);

    return iter == map.end() ? "" : iter->second;
}

VideoInput::VideoInput(const std::map<std::string, std::string>& map) :
    VideoGenerator::VideoGenerator()
    , id_(SINK_ID)
    , decoder_(0)
    , sink_()
    , mirror_(map.find("mirror") != map.end())
    , input_(extract(map, "input"))
    , loop_(extract(map, "loop"))
    , format_(extract(map, "format"))
    , channel_(extract(map, "channel"))
    , frameRate_(extract(map, "framerate"))
    , videoSize_(extract(map, "video_size"))
    , emulateRate_(map.find("emulate_rate") != map.end())
{
    DEBUG("initializing video input with: "
            "mirror: %s, "
            "input: '%s', "
            "format: '%s', "
            "channel: '%s', "
            "framerate: '%s', "
            "video_size: '%s', "
            "emulate_rate: '%s'",
            mirror_ ? "yes" : "no",
            input_.c_str(),
            format_.c_str(),
            channel_.c_str(),
            frameRate_.c_str(),
            videoSize_.c_str(),
            emulateRate_ ? "yes" : "no");

    start();
}

VideoInput::~VideoInput()
{
    stop();
    join();
}

bool VideoInput::setup()
{
    decoder_ = new VideoDecoder();

    if (!frameRate_.empty())
        decoder_->setOption("framerate", frameRate_.c_str());
    if (!videoSize_.empty())
        decoder_->setOption("video_size", videoSize_.c_str());
    if (!channel_.empty())
        decoder_->setOption("channel", channel_.c_str());
    if (!loop_.empty())
        decoder_->setOption("loop", loop_.c_str());
    if (emulateRate_)
        decoder_->emulateRate();

    decoder_->setInterruptCallback(interruptCb, this);

    EXIT_IF_FAIL(decoder_->openInput(input_, format_) >= 0,
                 "Could not open input \"%s\"", input_.c_str());

    /* Data available, finish the decoding */
    EXIT_IF_FAIL(!decoder_->setupFromVideoData(), "decoder IO startup failed");

    /* Sink setup */
    EXIT_IF_FAIL(sink_.start(), "Cannot start shared memory sink");
    if (attach(&sink_)) {
        Manager::instance().getVideoManager()->startedDecoding(id_, sink_.openedName(),
			decoder_->getWidth(), decoder_->getHeight(), false);
        DEBUG("LOCAL: shm sink <%s> started: size = %dx%d",
              sink_.openedName().c_str(), decoder_->getWidth(), decoder_->getHeight());
    }

    return true;
}

void VideoInput::process()
{ captureFrame(); }

void VideoInput::cleanup()
{
    if (detach(&sink_)) {
        Manager::instance().getVideoManager()->stoppedDecoding(id_, sink_.openedName(), false);
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

    if (mirror_)
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
