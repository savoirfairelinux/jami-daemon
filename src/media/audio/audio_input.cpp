/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
 *
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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

#include "audio_frame_resizer.h"
#include "audio_input.h"
#include "dring/media_const.h"
#include "fileutils.h" // access
#include "manager.h"
#include "media_decoder.h"
#include "resampler.h"
#include "ringbuffer.h"
#include "ringbufferpool.h"
#include "smartools.h"

#include <future>
#include <chrono>
#include <memory>

namespace jami {

static constexpr auto MS_PER_PACKET = std::chrono::milliseconds(20);

AudioInput::AudioInput(const std::string& id) :
    id_(id),
    format_(Manager::instance().getRingBufferPool().getInternalAudioFormat()),
    frameSize_(format_.sample_rate * MS_PER_PACKET.count() / 1000),
    resampler_(new Resampler),
    resizer_(new AudioFrameResizer(format_, frameSize_,
       [this](std::shared_ptr<AudioFrame>&& f){ frameResized(std::move(f)); })),
    fileId_(id + "_file"),
    loop_([] { return true; },
          [this] { process(); },
          [] {})
{
    JAMI_DBG() << "Creating audio input with id: " << id;
    loop_.start();
}

AudioInput::~AudioInput()
{
    loop_.join();
}

void
AudioInput::process()
{
    // NOTE This is only useful if the device params weren't yet found in switchInput
    // For both files and audio devices, this is already done
    //foundDevOpts(devOpts_);
    if (switchPending_.exchange(false)) {
        if (devOpts_.input.empty())
            JAMI_DBG() << "Switching to default audio input";
        else
            JAMI_DBG() << "Switching audio input to '" << devOpts_.input << "'";
    }

    readFromDevice();
}

void
AudioInput::frameResized(std::shared_ptr<AudioFrame>&& ptr)
{
    std::shared_ptr<AudioFrame> frame = std::move(ptr);
    frame->pointer()->pts = sent_samples;
    sent_samples += frame->pointer()->nb_samples;

    notify(std::static_pointer_cast<MediaFrame>(frame));
}

void
AudioInput::readFromDevice()
{
    auto& mainBuffer = Manager::instance().getRingBufferPool();
    auto bufferFormat = mainBuffer.getInternalAudioFormat();

    if (decodingFile_ )
        while (fileBuf_->isEmpty())
            readFromFile();

    if (not mainBuffer.waitForDataAvailable(id_, MS_PER_PACKET))
        return;

    auto samples = mainBuffer.getData(id_);
    if (not samples)
        return;

    if (muteState_)
        libav_utils::fillWithSilence(samples->pointer());

    std::lock_guard<std::mutex> lk(fmtMutex_);
    if (bufferFormat != format_)
        samples = resampler_->resample(std::move(samples), format_);
    resizer_->enqueue(std::move(samples));
}

void
AudioInput::readFromFile()
{
    if (!decoder_)
        return;
    const auto ret = decoder_->decode();
    switch (ret) {
    case MediaDemuxer::Status::Success:
        break;
    case MediaDemuxer::Status::EndOfFile:
        createDecoder();
        break;
    case MediaDemuxer::Status::ReadError:
        JAMI_ERR() << "Failed to decode frame";
        break;
    }
}

bool
AudioInput::initDevice(const std::string& device)
{
    devOpts_ = {};
    devOpts_.input = device;
    devOpts_.channel = format_.nb_channels;
    devOpts_.framerate = format_.sample_rate;
    return true;
}

bool
AudioInput::initFile(const std::string& path)
{
    if (access(path.c_str(), R_OK) != 0) {
        JAMI_ERR() << "File '" << path << "' not available";
        return false;
    }

    devOpts_ = {};
    devOpts_.input = path;
    devOpts_.name = path;
    devOpts_.loop = "1";
    // sets devOpts_'s sample rate and number of channels
    if (!createDecoder()) {
        JAMI_WARN() << "Cannot decode audio from file, switching back to default device";
        return initDevice("");
    }
    fileBuf_ = Manager::instance().getRingBufferPool().createRingBuffer(fileId_);
    // have file audio mixed into the call buffer so it gets sent to the peer
    Manager::instance().getRingBufferPool().bindHalfDuplexOut(id_, fileId_);
    // have file audio mixed into the local buffer so it gets played
    Manager::instance().getRingBufferPool().bindHalfDuplexOut(RingBufferPool::DEFAULT_ID, fileId_);
    decodingFile_ = true;
    return true;
}

std::shared_future<DeviceParams>
AudioInput::switchInput(const std::string& resource)
{
    // Always switch inputs, even if it's the same resource, so audio will be in sync with video

    if (switchPending_) {
        JAMI_ERR() << "Audio switch already requested";
        return {};
    }

    JAMI_DBG() << "Switching audio source to match '" << resource << "'";

    decoder_.reset();
    decodingFile_ = false;
    Manager::instance().getRingBufferPool().unBindHalfDuplexOut(id_, fileId_);
    Manager::instance().getRingBufferPool().unBindHalfDuplexOut(RingBufferPool::DEFAULT_ID, fileId_);
    fileBuf_.reset();

    currentResource_ = resource;
    devOptsFound_ = false;

    std::promise<DeviceParams> p;
    foundDevOpts_.swap(p);

    if (resource.empty()) {
        if (initDevice(""))
            foundDevOpts(devOpts_);
        switchPending_ = true;
        futureDevOpts_ = foundDevOpts_.get_future();
        return futureDevOpts_;
    }

    static const std::string sep = DRing::Media::VideoProtocolPrefix::SEPARATOR;

    const auto pos = resource.find(sep);
    if (pos == std::string::npos)
        return {};

    const auto prefix = resource.substr(0, pos);
    if ((pos + sep.size()) >= resource.size())
        return {};

    const auto suffix = resource.substr(pos + sep.size());
    bool ready = false;
    if (prefix == DRing::Media::VideoProtocolPrefix::FILE)
        ready = initFile(suffix);
    else
        ready = initDevice(suffix);

    if (ready)
        foundDevOpts(devOpts_);

    switchPending_ = true;
    futureDevOpts_ = foundDevOpts_.get_future().share();
    return futureDevOpts_;
}

void
AudioInput::foundDevOpts(const DeviceParams& params)
{
    if (!devOptsFound_) {
        devOptsFound_ = true;
        foundDevOpts_.set_value(params);
    }
}

bool
AudioInput::createDecoder()
{
    decoder_.reset();
    if (devOpts_.input.empty()) {
        foundDevOpts(devOpts_);
        return false;
    }

    auto decoder = std::make_unique<MediaDecoder>([this](std::shared_ptr<MediaFrame>&& frame) {
        fileBuf_->put(std::move(std::static_pointer_cast<AudioFrame>(frame)));
    });

    // NOTE don't emulate rate, file is read as frames are needed

    decoder->setInterruptCallback(
        [](void* data) -> int { return not static_cast<AudioInput*>(data)->isCapturing(); }, this);

    if (decoder->openInput(devOpts_) < 0) {
        JAMI_ERR() << "Could not open input '" << devOpts_.input << "'";
        foundDevOpts(devOpts_);
        return false;
    }

    if (decoder->setupAudio() < 0) {
        JAMI_ERR() << "Could not setup decoder for '" << devOpts_.input << "'";
        foundDevOpts(devOpts_);
        return false;
    }

    auto ms = decoder->getStream(devOpts_.input);
    devOpts_.channel = ms.nbChannels;
    devOpts_.framerate = ms.sampleRate;
    JAMI_DBG() << "Created audio decoder: " << ms;

    decoder_ = std::move(decoder);
    foundDevOpts(devOpts_);
    return true;
}

void
AudioInput::setFormat(const AudioFormat& fmt)
{
    std::lock_guard<std::mutex> lk(fmtMutex_);
    format_ = fmt;
    resizer_->setFormat(format_, format_.sample_rate * MS_PER_PACKET.count() / 1000);
}

void
AudioInput::setMuted(bool isMuted)
{
    muteState_ = isMuted;
}

MediaStream
AudioInput::getInfo() const
{
    std::lock_guard<std::mutex> lk(fmtMutex_);
    auto ms = MediaStream("a:local", format_, sent_samples);
    return ms;
}

} // namespace jami
