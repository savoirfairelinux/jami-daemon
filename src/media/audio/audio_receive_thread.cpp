/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "audio_receive_thread.h"
#include "libav_deps.h"
#include "logger.h"
#include "manager.h"
#include "media_decoder.h"
#include "media_io_handle.h"
#include "media_recorder.h"
#include "ringbuffer.h"
#include "ringbufferpool.h"
#include "smartools.h"

#include <memory>

namespace jami {

AudioReceiveThread::AudioReceiveThread(std::string peerUri,
                                       const std::string& id,
                                       const AudioFormat& format,
                                       const std::string& sdp,
                                       const uint16_t mtu)
    : Observable("AudioReceiver_" + std::move(peerUri))
    , id_(id)
    , format_(format)
    , stream_(sdp)
    , sdpContext_(new MediaIOHandle(sdp.size(), false, &readFunction,
                                    0, 0, this))
    , mtu_(mtu)
    , loop_(std::bind(&AudioReceiveThread::setup, this),
            std::bind(&AudioReceiveThread::process, this),
            std::bind(&AudioReceiveThread::cleanup, this))
{}

AudioReceiveThread::~AudioReceiveThread()
{
    loop_.join();
}

bool
AudioReceiveThread::setup()
{
    audioDecoder_.reset(new MediaDecoder([this](std::shared_ptr<MediaFrame>&& frame) mutable {
        notify(frame);
        ringbuffer_->put(std::move(std::static_pointer_cast<AudioFrame>(frame)));
    }));
    audioDecoder_->setInterruptCallback(interruptCb, this);

    // custom_io so the SDP demuxer will not open any UDP connections
    args_.input = SDP_FILENAME;
    args_.format = "sdp";
    args_.sdp_flags = "custom_io";

    if (stream_.str().empty()) {
        JAMI_ERR("No SDP loaded");
        return false;
    }

    audioDecoder_->setIOContext(sdpContext_.get());
    if (audioDecoder_->openInput(args_)) {
        JAMI_ERR("Could not open input \"%s\"", SDP_FILENAME);
        return false;
    }

    // Now replace our custom AVIOContext with one that will read packets
    audioDecoder_->setIOContext(demuxContext_.get());
    if (audioDecoder_->setupAudio()) {
        JAMI_ERR("decoder IO startup failed");
        return false;
    }
    Smartools::getInstance().setRemoteAudioCodec(audioDecoder_->getDecoderName());

    ringbuffer_ = Manager::instance().getRingBufferPool().getRingBuffer(id_);

    if (onSetupSuccess_)
        onSetupSuccess_(MEDIA_AUDIO);

    return true;
}

void
AudioReceiveThread::process()
{
    audioDecoder_->decode();
}

void
AudioReceiveThread::cleanup()
{
    audioDecoder_.reset();
    demuxContext_.reset();
}

int
AudioReceiveThread::readFunction(void* opaque, uint8_t* buf, int buf_size)
{
    std::istream& is = static_cast<AudioReceiveThread*>(opaque)->stream_;
    is.read(reinterpret_cast<char*>(buf), buf_size);

    auto count = is.gcount();
    return count ? count : AVERROR_EOF;
}

// This callback is used by libav internally to break out of blocking calls
int
AudioReceiveThread::interruptCb(void* data)
{
    auto context = static_cast<AudioReceiveThread*>(data);
    return not context->loop_.isRunning();
}

void
AudioReceiveThread::addIOContext(SocketPair& socketPair)
{
    demuxContext_.reset(socketPair.createIOContext(mtu_));
}

MediaStream
AudioReceiveThread::getInfo() const
{
    return audioDecoder_->getStream("a:remote");
}

void
AudioReceiveThread::startLoop(const std::function<void(MediaType)>& cb)
{
    onSetupSuccess_ = cb;
    loop_.start();
}

}; // namespace jami
