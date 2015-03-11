/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
#include "video_receive_thread.h"
#include "media/media_decoder.h"
#include "socket_pair.h"
#include "manager.h"
#include "client/videomanager.h"
#include "client/signal.h"
#include "sinkclient.h"
#include "logger.h"

#include <unistd.h>
#include <map>

namespace ring { namespace video {

using std::string;

VideoReceiveThread::VideoReceiveThread(const std::string& id,
                                       const std::string &sdp) :
    VideoGenerator::VideoGenerator()
    , args_()
    , dstWidth_(0)
    , dstHeight_(0)
    , id_(id)
    , stream_(sdp)
    , sdpContext_(stream_.str().size(), false, &readFunction, 0, 0, this)
    , sink_ {Manager::instance().createSinkClient(id)}
    , requestKeyFrameCallback_(0)
    , loop_(std::bind(&VideoReceiveThread::setup, this),
            std::bind(&VideoReceiveThread::process, this),
            std::bind(&VideoReceiveThread::cleanup, this))
{}

VideoReceiveThread::~VideoReceiveThread()
{
    loop_.join();
}

void
VideoReceiveThread::startLoop()
{
    loop_.start();
}

// We do this setup here instead of the constructor because we don't want the
// main thread to block while this executes, so it happens in the video thread.
bool VideoReceiveThread::setup()
{
    videoDecoder_.reset(new MediaDecoder());

    dstWidth_ = args_.width;
    dstHeight_ = args_.height;

    const std::string SDP_FILENAME = "dummyFilename";
    if (args_.input.empty()) {
        args_.format = "sdp";
        args_.input = SDP_FILENAME;
    } else if (args_.input.substr(0, strlen("/dev/video")) == "/dev/video") {
        // it's a v4l device if starting with /dev/video
        // FIXME: This is not a robust way of checking if we mean to use a
        // v4l2 device
        args_.format = "video4linux2";
    }

    videoDecoder_->setInterruptCallback(interruptCb, this);

    if (args_.input == SDP_FILENAME) {
        // Force custom_io so the SDP demuxer will not open any UDP connections
        // We need it to use ICE transport.
        args_.sdp_flags = "custom_io";

        EXIT_IF_FAIL(not stream_.str().empty(), "No SDP loaded");
        videoDecoder_->setIOContext(&sdpContext_);
    }

    //videoDecoder_->setOptions(args_);

    EXIT_IF_FAIL(!videoDecoder_->openInput(args_),
                 "Could not open input \"%s\"", args_.input.c_str());

    if (args_.input == SDP_FILENAME) {
        // Now replace our custom AVIOContext with one that will read packets
        videoDecoder_->setIOContext(demuxContext_.get());
    }

    // FIXME: this is a hack because our peer sends us RTP before
    // we're ready for it, and we miss the SPS/PPS. We should be
    // ready earlier.
    sleep(1);
    if (requestKeyFrameCallback_)
        requestKeyFrameCallback_(id_);

    EXIT_IF_FAIL(!videoDecoder_->setupFromVideoData(),
                 "decoder IO startup failed");

    // Default size from input video
    if (dstWidth_ == 0 and dstHeight_ == 0) {
        dstWidth_ = videoDecoder_->getWidth();
        dstHeight_ = videoDecoder_->getHeight();
    }

    EXIT_IF_FAIL(sink_->start(), "RX: sink startup failed");

    auto conf = Manager::instance().getConferenceFromCallID(id_);
    if (!conf)
        exitConference();

    return true;
}

void VideoReceiveThread::process()
{ decodeFrame(); }

void VideoReceiveThread::cleanup()
{
    if (detach(sink_.get()))
        emitSignal<DRing::VideoSignal::DecodingStopped>(id_,
                                                        sink_->openedName(),
                                                        false);
    sink_->stop();

    videoDecoder_.reset();
    demuxContext_.reset();
}

// This callback is used by libav internally to break out of blocking calls
int VideoReceiveThread::interruptCb(void *data)
{
    const auto context = static_cast<VideoReceiveThread*>(data);
    return not context->loop_.isRunning();
}

int VideoReceiveThread::readFunction(void *opaque, uint8_t *buf, int buf_size)
{
    std::istream &is = static_cast<VideoReceiveThread*>(opaque)->stream_;
    is.read(reinterpret_cast<char*>(buf), buf_size);
    return is.gcount();
}

void VideoReceiveThread::addIOContext(SocketPair &socketPair)
{
    demuxContext_.reset(socketPair.createIOContext());
}

bool VideoReceiveThread::decodeFrame()
{
    VideoPacket pkt;
    const auto ret = videoDecoder_->decode(getNewFrame(), pkt);

    switch (ret) {
        case MediaDecoder::Status::FrameFinished:
            publishFrame();
            return true;

        case MediaDecoder::Status::DecodeError:
            RING_WARN("video decoding failure");
            if (requestKeyFrameCallback_)
                requestKeyFrameCallback_(id_);
            break;

        case MediaDecoder::Status::ReadError:
            RING_ERR("fatal error, read failed");
            loop_.stop();

        default:
            break;
    }

    return false;
}


void VideoReceiveThread::enterConference()
{
    if (!loop_.isRunning())
        return;

    if (detach(sink_.get())) {
        emitSignal<DRing::VideoSignal::DecodingStopped>(id_,
                                                        sink_->openedName(),
                                                        false);
        RING_DBG("RX: shm sink <%s> detached", sink_->openedName().c_str());
    }
}

void VideoReceiveThread::exitConference()
{
    if (!loop_.isRunning())
        return;

    if (dstWidth_ > 0 && dstHeight_ > 0) {
        if (attach(sink_.get())) {
            emitSignal<DRing::VideoSignal::DecodingStarted>(id_,
                                                            sink_->openedName(),
                                                            dstWidth_,
                                                            dstHeight_, false);
            RING_DBG("RX: shm sink <%s> started: size = %dx%d",
                     sink_->openedName().c_str(), dstWidth_, dstHeight_);
        }
    }
}

void VideoReceiveThread::setRequestKeyFrameCallback(
    void (*cb)(const std::string &))
{ requestKeyFrameCallback_ = cb; }

int VideoReceiveThread::getWidth() const
{ return dstWidth_; }

int VideoReceiveThread::getHeight() const
{ return dstHeight_; }

int VideoReceiveThread::getPixelFormat() const
{ return videoDecoder_->getPixelFormat(); }

}} // namespace ring::video
