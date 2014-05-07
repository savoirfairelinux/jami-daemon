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

namespace sfl_video {

VideoInput::VideoInput() :
    VideoGenerator::VideoGenerator()
    , sink_()
{
    start();
}

VideoInput::~VideoInput()
{
    stop();
    join();
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

void
VideoInput::createDecoder()
{
    DEBUG("decoder params:"
            " channel:%s"
            " emulate_rate:%s"
            " format:%s"
            " framerate:%s"
            " input:%s"
            " loop:%s"
            " mirror:%s"
            " video_size:%s",
            channel_.c_str(),
            emulateRate_ ? "yes" : "no",
            format_.c_str(),
            frameRate_.c_str(),
            input_.c_str(),
            loop_.c_str(),
            mirror_ ? "yes" : "no",
            videoSize_.c_str());

    if (input_.empty())
        return;

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

    /* Signal the client about the new sink */
    Manager::instance().getVideoManager()->startedDecoding(id_, sink_.openedName(),
            decoder_->getWidth(), decoder_->getHeight(), false);
    DEBUG("LOCAL: shm sink <%s> started: size = %dx%d",
            sink_.openedName().c_str(), decoder_->getWidth(), decoder_->getHeight());
}

void
VideoInput::deleteDecoder()
{
    if (not decoder_)
        return;

    Manager::instance().getVideoManager()->stoppedDecoding(id_, sink_.openedName(), false);
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
    channel_ = map["channel"];
    frameRate_ = map["framerate"];
    videoSize_ = map["video_size"];

    format_ = "video4linux2";
    mirror_ = true;

    return true;
}

bool
VideoInput::initX11(std::string display)
{
    size_t space = display.find(' ');

    if (space != std::string::npos) {
        videoSize_ = display.substr(space + 1);
        input_ = display.erase(space);
    } else {
        input_ = display;
        videoSize_ = "vga";
    }

    format_ = "x11grab";
    frameRate_ = "25";

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
        frameRate_ = "1";
        loop_ = "1";
        emulateRate_ = true;
    } else {
        ERROR("unsupported filetype '%s'\n", ext.c_str());
        return false;
    }

    return true;
}

bool
VideoInput::switchInput(const std::string& resource)
{
    if (switchPending_) {
        ERROR("Video switch already requested");
        return false;
    }

    // Reset attributes
    channel_ = "";
    emulateRate_ = false;
    format_ = "";
    frameRate_ = "";
    input_ = "";
    loop_ = "";
    mirror_ = false;
    videoSize_ = "";

    if (resource.empty()) {
        switchPending_ = true;
        return true;
    }

    DEBUG("MRL: '%s'", resource.c_str());

    // Supported MRL schemes
    static const std::string v4l2("v4l2://");
    static const std::string display("display://");
    static const std::string file("file://");

    bool valid = false;

    /* Video4Linux2 */
    if (resource.compare(0, v4l2.size(), v4l2) == 0)
        valid = initCamera(resource.substr(v4l2.size()));

    /* X11 display name */
    else if (resource.compare(0, display.size(), display) == 0)
        valid = initX11(resource.substr(display.size()));

    /* Pathname */
    else if (resource.compare(0, file.size(), file) == 0)
        valid = initFile(resource.substr(file.size()));

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
