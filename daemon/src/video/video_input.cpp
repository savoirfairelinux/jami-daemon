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

#include "manager.h"
#include "client/videomanager.h"
#include "logger.h"

#include <map>
#include <string>

namespace sfl_video {

VideoInput::VideoInput() :
    VideoGenerator::VideoGenerator()
    , sink_()
    , loop_(std::bind(&VideoInput::setup, this),
            std::bind(&VideoInput::process, this),
            std::bind(&VideoInput::cleanup, this))
{
    loop_.start();
}

VideoInput::~VideoInput()
{
    loop_.join();
}

bool VideoInput::setup()
{
    /* Sink setup */
    EXIT_IF_FAIL(sink_.start(), "Cannot start shared memory sink");
    if (not attach(&sink_))
        WARN("Failed to attach sink");

    return true;
}

void VideoInput::process()
{
    if (switchPending_) {
        deleteDecoder();
        createDecoder();
        switchPending_ = false;
    }

    if (!decoder_) {
        ERROR("No decoder");
        loop_.stop();
        return;
    }

    captureFrame();
}

void VideoInput::cleanup()
{
    deleteDecoder();

    if (detach(&sink_))
        sink_.stop();
}

int VideoInput::interruptCb(void *data)
{
    VideoInput *context = static_cast<VideoInput*>(data);
    return not context->loop_.isRunning();
}

bool VideoInput::captureFrame()
{
    VideoFrame& frame = getNewFrame();
    const auto ret = decoder_->decode(frame);

    switch (ret) {
        case VideoDecoder::Status::FrameFinished:
            break;

        case VideoDecoder::Status::ReadError:
        case VideoDecoder::Status::DecodeError:
            loop_.stop();
            // fallthrough
        case VideoDecoder::Status::Success:
            return false;
    }

    if (mirror_)
        frame.mirror();
    publishFrame();
    return true;
}

void
VideoInput::createDecoder()
{
    if (input_.empty())
        return;

    decoder_ = new VideoDecoder();

    decoder_->setOptions(decOpts_);
    if (emulateRate_)
        decoder_->emulateRate();

    decoder_->setInterruptCallback(interruptCb, this);

    EXIT_IF_FAIL(decoder_->openInput(input_, format_) >= 0,
                 "Could not open input \"%s\"", input_.c_str());

    /* Data available, finish the decoding */
    EXIT_IF_FAIL(!decoder_->setupFromVideoData(), "decoder IO startup failed");

    /* Signal the client about the new sink */
    Manager::instance().getVideoManager()->startedDecoding(sinkID_, sink_.openedName(),
            decoder_->getWidth(), decoder_->getHeight(), false);
    DEBUG("LOCAL: shm sink <%s> started: size = %dx%d",
            sink_.openedName().c_str(), decoder_->getWidth(), decoder_->getHeight());
}

void
VideoInput::deleteDecoder()
{
    if (not decoder_)
        return;

    Manager::instance().getVideoManager()->stoppedDecoding(sinkID_,
                                                           sink_.openedName(),
                                                           false);
    delete decoder_;
    decoder_ = nullptr;
}

bool
VideoInput::initCamera(const std::string& device)
{
    std::map<std::string, std::string> map =
        Manager::instance().getVideoManager()->getSettingsFor(device);

    if (map.empty())
        return false;

    input_ = map["input"];
    format_ = "video4linux2";
    decOpts_.clear();
    decOpts_["channel"] = map["channel"];
    decOpts_["framerate"] = map["framerate"];
    decOpts_["video_size"] = map["video_size"];
    mirror_ = true;

    return true;
}

bool
VideoInput::initX11(std::string display)
{
    size_t space = display.find(' ');

    format_ = "x11grab";
    mirror_ = false;
    decOpts_.clear();
    decOpts_["framerate"] = "25";

    if (space != std::string::npos) {
        decOpts_["video_size"] = display.substr(space + 1);
        input_ = display.erase(space);
    } else {
        input_ = display;
        decOpts_["video_size"] = "vga";
    }

    return true;
}

bool
VideoInput::initFile(std::string path)
{
    size_t dot = path.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : path.substr(dot + 1);

    /* File exists? */
    if (access(path.c_str(), R_OK) != 0) {
        ERROR("file '%s' unavailable\n", path.c_str());
        return false;
    }

    /* Supported image? */
    if (ext == "jpeg" || ext == "jpg" || ext == "png") {
        input_ = path;
        format_ = "image2";
        emulateRate_ = true;

        decOpts_.clear();
        decOpts_["framerate"] = "1";
        decOpts_["loop"] = "1";
        return true;
    }

    ERROR("unsupported filetype '%s'\n", ext.c_str());
    return false;
}

bool
VideoInput::switchInput(const std::string& resource)
{
    DEBUG("MRL: '%s'", resource.c_str());

    if (switchPending_) {
        ERROR("Video switch already requested");
        return false;
    }

    // Switch off video input?
    if (resource.empty()) {
        input_.clear();
        switchPending_ = true;
        return true;
    }

    // Supported MRL schemes
    static const std::string sep = "://";

    const auto pos = resource.find(sep);
    if (pos == std::string::npos)
        return false;

    const auto prefix = resource.substr(0, pos);
    if ((prefix.size() + sep.size()) >= resource.size())
        return false;

    const auto suffix = resource.substr(prefix.size() + sep.size());

    bool valid = false;

    if (prefix == "v4l2") {
        /* Video4Linux2 */
        valid = initCamera(suffix);
    } else if (prefix == "display") {
        /* X11 display name */
        valid = initX11(suffix);
    } else if (prefix == "file") {
        /* Pathname */
        valid = initFile(suffix);
    }

    /* Unsupported MRL or failed initialization */
    if (valid)
        switchPending_ = true;
    else
        ERROR("Failed to init input for MRL '%s'\n", resource.c_str());

    return valid;
}

int VideoInput::getWidth() const
{ return decoder_->getWidth(); }

int VideoInput::getHeight() const
{ return decoder_->getHeight(); }

int VideoInput::getPixelFormat() const
{ return decoder_->getPixelFormat(); }

} // end namespace sfl_video
