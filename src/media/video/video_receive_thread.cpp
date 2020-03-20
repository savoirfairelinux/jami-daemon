/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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
 */

#include "libav_deps.h" // MUST BE INCLUDED FIRST
#include "video_receive_thread.h"
#include "media/media_decoder.h"
#include "socket_pair.h"
#include "manager.h"
#include "client/videomanager.h"
#include "sinkclient.h"
#include "logger.h"
#include "smartools.h"

extern "C" {
#include <libavutil/display.h>
}

#include <unistd.h>
#include <map>

namespace jami { namespace video {

using std::string;

VideoReceiveThread::VideoReceiveThread(const std::string& id,
                                       const std::string &sdp,
                                       uint16_t mtu) :
    VideoGenerator::VideoGenerator()
    , args_()
    , dstWidth_(0)
    , dstHeight_(0)
    , id_(id)
    , stream_(sdp)
    , sdpContext_(stream_.str().size(), false, &readFunction, 0, 0, this)
    , sink_ {Manager::instance().createSinkClient(id)}
    , isVideoConfigured_(false)
    , mtu_(mtu)
    , rotation_(0)
    , loop_(std::bind(&VideoReceiveThread::setup, this),
            std::bind(&VideoReceiveThread::decodeFrame, this),
            std::bind(&VideoReceiveThread::cleanup, this))
{}

VideoReceiveThread::~VideoReceiveThread()
{
    loop_.join();
}

void
VideoReceiveThread::startLoop(const std::function<void(MediaType)>& cb)
{
    onSetupSuccess_ = cb;
    loop_.start();
}

// We do this setup here instead of the constructor because we don't want the
// main thread to block while this executes, so it happens in the video thread.
bool VideoReceiveThread::setup()
{
    videoDecoder_.reset(new MediaDecoder([this](const std::shared_ptr<MediaFrame>& frame) mutable {
        if (auto displayMatrix = displayMatrix_)
            av_frame_new_side_data_from_buf(frame->pointer(), AV_FRAME_DATA_DISPLAYMATRIX, av_buffer_ref(displayMatrix.get()));
        publishFrame(std::static_pointer_cast<VideoFrame>(frame));
    }));

    dstWidth_ = args_.width;
    dstHeight_ = args_.height;

    static const std::string SDP_FILENAME = "dummyFilename";
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

        if (stream_.str().empty()) {
            JAMI_ERR("No SDP loaded");
            return false;
        }

        videoDecoder_->setIOContext(&sdpContext_);
    }

    if (videoDecoder_->openInput(args_)) {
        JAMI_ERR("Could not open input \"%s\"", args_.input.c_str());
        return false;
    }

    if (args_.input == SDP_FILENAME) {
        // Now replace our custom AVIOContext with one that will read packets
        videoDecoder_->setIOContext(demuxContext_.get());
    }
    return true;
}

void VideoReceiveThread::cleanup()
{
    detach(sink_.get());
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

    auto count = is.gcount();
    if (count != 0)
        return count;
    else
        return AVERROR_EOF;
}

void VideoReceiveThread::addIOContext(SocketPair& socketPair)
{
    demuxContext_.reset(socketPair.createIOContext(mtu_));
}

void VideoReceiveThread::decodeFrame()
{
    if (!configureVideoOutput()) {
        return;
    }
    auto status = videoDecoder_->decode();
    if (status == MediaDemuxer::Status::EndOfFile ||
        status == MediaDemuxer::Status::ReadError) {
        loop_.stop();
    }
    if (status == MediaDemuxer::Status::FallBack) {
        if (keyFrameRequestCallback_)
            keyFrameRequestCallback_();
    }
}

bool VideoReceiveThread::configureVideoOutput()
{
    if (isVideoConfigured_) {
        return true;
    }
    if (!loop_.isRunning()) {
        return false;
    }
   if (videoDecoder_->setupVideo()) {
        JAMI_ERR("decoder IO startup failed");
        loop_.stop();
        return false;
    }

    // Default size from input video
    if (dstWidth_ == 0 and dstHeight_ == 0) {
        dstWidth_ = videoDecoder_->getWidth();
        dstHeight_ = videoDecoder_->getHeight();
    }

    if (not sink_->start()) {
        JAMI_ERR("RX: sink startup failed");
        loop_.stop();
        return false;
    }

    auto conf = Manager::instance().getConferenceFromCallID(id_);
    if (!conf)
        exitConference();

    // Send remote video codec in SmartInfo
    Smartools::getInstance().setRemoteVideoCodec(videoDecoder_->getDecoderName(), id_);

    // Send the resolution in smartInfo
    Smartools::getInstance().setResolution(id_, dstWidth_, dstHeight_);

    if (onSetupSuccess_)
        onSetupSuccess_(MEDIA_VIDEO);
    isVideoConfigured_ = true;
    return true;
}

void VideoReceiveThread::enterConference()
{
    if (!loop_.isRunning())
        return;

    detach(sink_.get());
    sink_->setFrameSize(0, 0);
}

void VideoReceiveThread::exitConference()
{
    if (!loop_.isRunning())
        return;

    if (dstWidth_ > 0 and dstHeight_ > 0 and attach(sink_.get()))
        sink_->setFrameSize(dstWidth_, dstHeight_);
}

int VideoReceiveThread::getWidth() const
{ return dstWidth_; }

int VideoReceiveThread::getHeight() const
{ return dstHeight_; }

AVPixelFormat VideoReceiveThread::getPixelFormat() const
{ return videoDecoder_->getPixelFormat(); }

MediaStream
VideoReceiveThread::getInfo() const
{
    return videoDecoder_->getStream("v:remote");
}

void
VideoReceiveThread::setRotation(int angle)
{
    std::shared_ptr<AVBufferRef> displayMatrix {
        av_buffer_alloc(sizeof(int32_t) * 9),
        [](AVBufferRef* buf){ av_buffer_unref(&buf); }
    };
    if (displayMatrix) {
        av_display_rotation_set(reinterpret_cast<int32_t*>(displayMatrix->data), angle);
        displayMatrix_ = std::move(displayMatrix);
    }
}

}} // namespace jami::video
