/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Eloi Bail <eloi.bail@savoirfairelinux.com>
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

#include "audio_input.h"
#include "media_decoder.h"
#include "manager.h"
#include "client/ring_signal.h"
#include "logger.h"
#include "audio/audiobuffer.h"
#include "media_buffer.h"
#include "audio/ringbufferpool.h"
#include "audio/ringbuffer.h"

#include <map>
#include <string>
#include <sstream>
#include <cassert>
#include <unistd.h>

namespace ring {

AudioInput::AudioInput()
    : mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
    , loop_(std::bind(&AudioInput::setup, this),
            std::bind(&AudioInput::process, this),
            std::bind(&AudioInput::cleanup, this))
{}

AudioInput::~AudioInput()
{
    loop_.join();
}

bool AudioInput::setup()
{
    ringbuffer_ = Manager::instance().getRingBufferPool().getRingBuffer(id_);
    return true;
}

void AudioInput::process()
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

#if 0
    if (newDecoderCreated) {
        /* Signal the client about the new sink */
        emitSignal<DRing::VideoSignal::DecodingStarted>(sink_->getId(),
                                                        sink_->openedName(),
                                                        decoder_->getWidth(),
                                                        decoder_->getHeight(),
                                                        false);
        RING_DBG("LOCAL: shm sink <%s> started: size = %dx%d",
                 sink_->openedName().c_str(), decoder_->getWidth(),
                 decoder_->getHeight());
    }
#endif
}

void AudioInput::cleanup()
{
    deleteDecoder();

    /*
    if (detach(sink_.get()))
        sink_->stop();
    */
}

void AudioInput::setRingBufferId(std::string id)
{
    id_ = id;
}

void AudioInput::clearOptions()
{
    decOpts_ = {};
    emulateRate_ = false;
}

int AudioInput::interruptCb(void *data)
{
    AudioInput *context = static_cast<AudioInput*>(data);
    return not context->loop_.isRunning();
}

bool AudioInput::captureFrame()
{
    RING_WARN("ELOI > %s",__FUNCTION__);
    AudioFormat mainBuffFormat = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    AudioFrame decodedFrame;

    switch (decoder_->decode(decodedFrame)) {

        case MediaDecoder::Status::FrameFinished:
            decoder_->writeToRingBuffer(decodedFrame, *ringbuffer_,
                                             mainBuffFormat);
            return true;

        case MediaDecoder::Status::DecodeError:
            RING_WARN("decoding failure, trying to reset decoder...");
            if (not setup()) {
                RING_ERR("fatal error, rx thread re-setup failed");
                loop_.stop();
                break;
            }
            if (not decoder_->setupFromAudioData(mainBuffFormat)) {
                RING_ERR("fatal error, a-decoder setup failed");
                loop_.stop();
                break;
            }
            break;

        case MediaDecoder::Status::ReadError:
            RING_ERR("fatal error, read failed");
            loop_.stop();
            break;

        case MediaDecoder::Status::Success:
            return false;

            // Play in loop
        case MediaDecoder::Status::EOFError:
            deleteDecoder();
            createDecoder();
            return false;
        default:
            break;
    }
}

void
AudioInput::createDecoder()
{
    if (decOpts_.input.empty()) {
        foundDecOpts(decOpts_);
        return;
    }

    decoder_ = new MediaDecoder();

    if (emulateRate_)
        decoder_->emulateRate();

    decoder_->setInterruptCallback(interruptCb, this);

    if (decoder_->openInput(decOpts_) < 0) {
        RING_ERR("Could not open input \"%s\"", decOpts_.input.c_str());
        delete decoder_;
        decoder_ = nullptr;
        //foundDecOpts_.set_exception(std::runtime_error("Could not open input"));
        foundDecOpts(decOpts_);
        return;
    }

    /* Data available, finish the decoding */
    if (decoder_->setupFromAudioData(Manager::instance().getRingBufferPool().getInternalAudioFormat()) < 0) {
        RING_ERR("decoder IO startup failed");
        delete decoder_;
        decoder_ = nullptr;
        //foundDecOpts_.set_exception(std::runtime_error("Could not read data"));
        foundDecOpts(decOpts_);
        return;
    }
    RING_INFO("create decoder audio");

    foundDecOpts(decOpts_);
}

void
AudioInput::deleteDecoder()
{
    if (not decoder_)
        return;

    /*
    emitSignal<DRing::VideoSignal::DecodingStopped>(sink_->getId(),
                                                    sink_->openedName(),
                                                    false);
    */
    //flushFrames();
    delete decoder_;
    decoder_ = nullptr;
}


bool
AudioInput::initFile(std::string path)
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

    return true;
}

std::shared_future<DeviceParams>
AudioInput::switchInput(const std::string& resource)
{
    if (resource == currentResource_)
        return futureDecOpts_;

    RING_DBG("MRL: '%s'", resource.c_str());

    if (switchPending_) {
        RING_ERR("Video switch already requested");
        return {};
    }

    currentResource_ = resource;
/*
    decOptsFound_ = false;
    std::promise<DeviceParams> p;
    foundDecOpts_.swap(p);

    // Switch off video input?
    if (resource.empty()) {
        clearOptions();
        switchPending_ = true;
        if (!loop_.isRunning())
            loop_.start();
        futureDecOpts_   = foundDecOpts_.get_future();
        return futureDecOpts_;
    }
*/

    // Supported MRL schemes
    static const std::string sep = "://";

    RING_ERR("ELOI BE LA");
    const auto pos = resource.find(sep);
    if (pos == std::string::npos)
        return {};

    const auto prefix = resource.substr(0, pos);
    if ((pos + sep.size()) >= resource.size())
        return {};

    const auto suffix = resource.substr(pos + sep.size());

    bool valid = false;

    RING_ERR("ELOI BE LA 2");
    if (prefix == "file") {
        /* Pathname */
        valid = initFile(suffix);
    }

    // Unsupported MRL or failed initialization
    if (not valid) {
        RING_ERR("Failed to init input for MRL '%s'\n", resource.c_str());
        return {};
    }

    switchPending_ = true;
    if (!loop_.isRunning()) {
        RING_ERR("ELOI Starting loop");
        loop_.start();
    }
    futureDecOpts_ = foundDecOpts_.get_future().share();
    RING_ERR("return futureDecOpts_");
    return futureDecOpts_;
}

DeviceParams AudioInput::getParams() const
{ return decOpts_; }

void
AudioInput::foundDecOpts(const DeviceParams& params)
{
    if (not decOptsFound_) {
        decOptsFound_ = true;
        foundDecOpts_.set_value(params);
    }
}

} // namespace ring
