/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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


#ifdef RING_VIDEO
#include "video_input.h"
#endif // RING_VIDEO

#include "media_decoder.h"
#include "manager.h"
#include "client/videomanager.h"
#include "client/signal.h"
#include "logger.h"

#include <map>
#include <string>
#include <cassert>
#include <unistd.h>

namespace ring { namespace video {

VideoInput::VideoInput() :
    VideoGenerator::VideoGenerator()
    , sink_()
    , loop_(std::bind(&VideoInput::setup, this),
            std::bind(&VideoInput::process, this),
            std::bind(&VideoInput::cleanup, this))
{}

VideoInput::~VideoInput()
{
    loop_.join();
}

bool VideoInput::setup()
{
    /* Sink setup */
    if (!sink_.start()) {
        RING_ERR("Cannot start shared memory sink");
        return false;
    }
    if (not attach(&sink_))
        RING_WARN("Failed to attach sink");

    return true;
}

void VideoInput::process()
{
    bool newDecoderCreated = false;

    if (switchPending_.exchange(false)) {
        deleteDecoder();
        createDecoder();
        newDecoderCreated = true;
    }

    if (not decoder_) {
        loop_.stop();
        return;
    }

    captureFrame();

    if (newDecoderCreated) {
        /* Signal the client about the new sink */
        emitSignal<DRing::VideoSignal::DecodingStarted>(sinkID_, sink_.openedName(),
                    decoder_->getWidth(), decoder_->getHeight(), false);
        RING_DBG("LOCAL: shm sink <%s> started: size = %dx%d",
              sink_.openedName().c_str(), decoder_->getWidth(),
              decoder_->getHeight());
    }
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
    VideoPacket pkt;
    const auto ret = decoder_->decode(getNewFrame(), pkt);

    switch (ret) {
        case MediaDecoder::Status::FrameFinished:
            break;

        case MediaDecoder::Status::ReadError:
        case MediaDecoder::Status::DecodeError:
            loop_.stop();
            // fallthrough
        case MediaDecoder::Status::Success:
            return false;

            // Play in loop
        case MediaDecoder::Status::EOFError:
            deleteDecoder();
            createDecoder();
            return false;
    }

    publishFrame();
    return true;
}

void
VideoInput::createDecoder()
{
    if (decOpts_.input.empty())
        return;

    decoder_ = new MediaDecoder();

    //decoder_->setOptions(decOpts_);
    if (emulateRate_)
        decoder_->emulateRate();

    decoder_->setInterruptCallback(interruptCb, this);

    if (decoder_->openInput(decOpts_ /*, format_*/) < 0) {
        RING_ERR("Could not open input \"%s\"", decOpts_.input.c_str());
        delete decoder_;
        decoder_ = nullptr;
        return;
    }

    /* Data available, finish the decoding */
    if (decoder_->setupFromVideoData() < 0) {
        RING_ERR("decoder IO startup failed");
        delete decoder_;
        decoder_ = nullptr;
        return;
    }
}

void
VideoInput::deleteDecoder()
{
    if (not decoder_)
        return;

    emitSignal<DRing::VideoSignal::DecodingStopped>(sinkID_, sink_.openedName(), false);
    flushFrames();
    delete decoder_;
    decoder_ = nullptr;
}

bool
VideoInput::initCamera(const std::string& device)
{
    auto map = DRing::getSettings(device);
    if (map.empty())
        return false;

    clearOptions();
    decOpts_.input = map["input"];
    decOpts_.format = "video4linux2";

    decOpts_.channel = map["channel_num"];
    decOpts_.framerate = std::stoi(map["framerate"]);
    decOpts_.video_size = map["video_size"];

    return true;
}

bool
VideoInput::initX11(std::string display)
{
    size_t space = display.find(' ');

    clearOptions();
    decOpts_.format = "x11grab";
    decOpts_.framerate = 25;

    if (space != std::string::npos) {
        decOpts_.video_size = display.substr(space + 1);
        decOpts_.input = display.erase(space);
    } else {
        decOpts_.input = display;
        decOpts_.video_size = "vga";
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
        RING_ERR("file '%s' unavailable\n", path.c_str());
        return false;
    }

    clearOptions();
    emulateRate_ = true;
    decOpts_.input = path;
    decOpts_.loop = "1";

    // Force 1fps for static image
    if (ext == "jpeg" || ext == "jpg" || ext == "png") {
        decOpts_.format = "image2";
        decOpts_.framerate = 1;
    } else {
        RING_WARN("Guessing file type for %s", path.c_str());
        // FIXME: proper parsing of FPS etc. should be done in
        // MediaDecoder, not here.
        decOpts_.framerate = 25;
    }

    return true;
}

bool
VideoInput::switchInput(const std::string& resource)
{
    RING_DBG("MRL: '%s'", resource.c_str());

    if (switchPending_) {
        RING_ERR("Video switch already requested");
        return false;
    }

    // Switch off video input?
    if (resource.empty()) {
        clearOptions();
        switchPending_ = true;
        if (!loop_.isRunning())
            loop_.start();
        return true;
    }

    // Supported MRL schemes
    static const std::string sep = "://";

    const auto pos = resource.find(sep);
    if (pos == std::string::npos)
        return false;

    const auto prefix = resource.substr(0, pos);
    if ((pos + sep.size()) >= resource.size())
        return false;

    const auto suffix = resource.substr(pos + sep.size());

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
    if (valid) {
        switchPending_ = true;
        if (!loop_.isRunning())
            loop_.start();
    }
    else
        RING_ERR("Failed to init input for MRL '%s'\n", resource.c_str());

    return valid;
}

int VideoInput::getWidth() const
{ return decoder_->getWidth(); }

int VideoInput::getHeight() const
{ return decoder_->getHeight(); }

int VideoInput::getPixelFormat() const
{ return decoder_->getPixelFormat(); }

}} // namespace ring::video
