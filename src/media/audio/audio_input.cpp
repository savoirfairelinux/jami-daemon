/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
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

#include "audio_input.h"
#include "audio_queue.h"
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

namespace ring {

static constexpr auto MS_PER_PACKET = std::chrono::milliseconds(20);

AudioInput::AudioInput(const std::string& id) :
    id_(id),
    format_(Manager::instance().getRingBufferPool().getInternalAudioFormat()),
    frameSize_(format_.sample_rate * MS_PER_PACKET.count() / 1000),
    resampler_(new Resampler),
    queue_(new AudioQueue(format_)),
    loop_([] { return true; },
          [this] { process(); },
          [] {})
{
    RING_DBG() << "Creating audio input with id: " << id;
    loop_.start();
}

AudioInput::~AudioInput()
{
    if (auto rec = recorder_.lock())
        rec->stopRecording();
    loop_.join();
}

void
AudioInput::process()
{
    foundDevOpts(devOpts_);
    if (switchPending_.exchange(false)) {
        if (devOpts_.input.empty())
            RING_DBG() << "Switching to default audio input";
        else
            RING_DBG() << "Switching audio input to '" << devOpts_.input << "'";
    }

    if (decodingFile_) {
        while (queue_->getSize() < frameSize_)
            nextFromFile();

        // TODO optimize this part
        if (auto deviceFrame = nextFromDevice()) {
            std::lock_guard<std::mutex> lk(fmtMutex_);
            auto deviceBuf = AudioBuffer(0, format_);
            deviceBuf.append(*deviceFrame);
            auto fileFrame = queue_->dequeue(frameSize_);
            auto fileBuf = AudioBuffer(0, format_);
            fileBuf.append(*fileFrame);
            deviceBuf.mix(fileBuf);
            std::shared_ptr<AudioFrame> frame = std::move(deviceBuf.toAVFrame());
            notify(frame);
        }
    } else {
        if (auto frame = nextFromDevice()) {
            std::shared_ptr<AudioFrame> sharedFrame = std::move(frame);
            notify(sharedFrame);
        }
    }
}

std::unique_ptr<AudioFrame>
AudioInput::nextFromDevice()
{
    auto& mainBuffer = Manager::instance().getRingBufferPool();
    auto bufferFormat = mainBuffer.getInternalAudioFormat();

    // compute number of samples contained in a frame with duration MS_PER_PACKET
    const auto samplesPerPacket = MS_PER_PACKET * bufferFormat.sample_rate;
    const std::size_t samplesToGet = std::chrono::duration_cast<std::chrono::seconds>(samplesPerPacket).count();

    if (mainBuffer.availableForGet(id_) < samplesToGet
        && not mainBuffer.waitForDataAvailable(id_, samplesToGet, MS_PER_PACKET)) {
        return nullptr;
    }

    // getData resets the format to internal hardware format, will have to be resampled
    micData_.setFormat(bufferFormat);
    micData_.resize(samplesToGet);
    const auto samples = mainBuffer.getData(micData_, id_);
    if (samples != samplesToGet)
        return nullptr;

    if (muteState_) // audio is muted, set samples to 0
        micData_.reset();

    std::lock_guard<std::mutex> lk(fmtMutex_);
    AudioBuffer resampled;
    resampled.setFormat(format_);
    if (bufferFormat != format_) {
        resampler_->resample(micData_, resampled);
    } else {
        resampled = micData_;
    }

    return resampled.toAVFrame();
}

void
AudioInput::nextFromFile()
{
    if (!decoder_)
        return;

    AudioFrame frame;
    const auto ret = decoder_->decode(frame);
    const auto inFmt = AudioFormat((unsigned)frame.pointer()->sample_rate, (unsigned)frame.pointer()->channels, (AVSampleFormat)frame.pointer()->format);

    std::lock_guard<std::mutex> lk(fmtMutex_);
    switch(ret) {
    case MediaDecoder::Status::ReadError:
    case MediaDecoder::Status::DecodeError:
        RING_ERR() << "Failed to decode frame";
        break;
    case MediaDecoder::Status::RestartRequired:
    case MediaDecoder::Status::EOFError:
        createDecoder();
        break;
    case MediaDecoder::Status::FrameFinished:
        if (inFmt != format_) {
            AudioFrame out;
            out.pointer()->format = format_.sampleFormat;
            out.pointer()->sample_rate = format_.sample_rate;
            out.pointer()->channel_layout = av_get_default_channel_layout(format_.nb_channels);
            out.pointer()->channels = format_.nb_channels;
            resampler_->resample(frame.pointer(), out.pointer());
            frame.copyFrom(out);
        }
        queue_->enqueue(frame);
        break;
    case MediaDecoder::Status::Success:
    default:
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
        RING_ERR() << "File '" << path << "' not available";
        return false;
    }

    devOpts_ = {};
    devOpts_.input = path;
    devOpts_.loop = "1";
    decodingFile_ = true;
    createDecoder(); // sets devOpts_'s sample rate and number of channels
    return true;
}

std::shared_future<DeviceParams>
AudioInput::switchInput(const std::string& resource)
{
    if (resource == currentResource_)
        return futureDevOpts_;

    if (switchPending_) {
        RING_ERR() << "Audio switch already requested";
        return {};
    }

    RING_DBG() << "Switching audio source to match '" << resource << "'";

    decoder_.reset();
    decodingFile_ = false;

    currentResource_ = resource;
    devOptsFound_ = false;

    std::promise<DeviceParams> p;
    foundDevOpts_.swap(p);

    if (resource.empty()) {
        devOpts_ = {};
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

void
AudioInput::createDecoder()
{
    decoder_.reset();
    if (devOpts_.input.empty()) {
        foundDevOpts(devOpts_);
        return;
    }

    // NOTE createDecoder is currently only used for files, which require rate emulation
    auto decoder = std::make_unique<MediaDecoder>();
    decoder->emulateRate();
    decoder->setInterruptCallback(
        [](void* data) -> int { return not static_cast<AudioInput*>(data)->isCapturing(); },
        this);

    if (decoder->openInput(devOpts_) < 0) {
        RING_ERR() << "Could not open input '" << devOpts_.input << "'";
        foundDevOpts(devOpts_);
        return;
    }

    if (decoder->setupFromAudioData() < 0) {
        RING_ERR() << "Could not setup decoder for '" << devOpts_.input << "'";
        foundDevOpts(devOpts_);
        return;
    }

    auto ms = decoder->getStream(devOpts_.input);
    devOpts_.channel = ms.nbChannels;
    devOpts_.framerate = ms.sampleRate;
    RING_DBG() << "Created audio decoder: " << ms;

    decoder_ = std::move(decoder);
    foundDevOpts(devOpts_);
}

void
AudioInput::setFormat(const AudioFormat& fmt)
{
    std::lock_guard<std::mutex> lk(fmtMutex_);
    format_ = fmt;
    frameSize_ = format_.sample_rate * MS_PER_PACKET.count() / 1000;
    // queue will not accept new format as input, so reset
    queue_.reset(new AudioQueue(format_));
}

void
AudioInput::setMuted(bool isMuted)
{
    muteState_ = isMuted;
}

void
AudioInput::initRecorder(const std::shared_ptr<MediaRecorder>& rec)
{
    rec->incrementExpectedStreams(1);
    recorder_ = rec;
}

} // namespace ring
