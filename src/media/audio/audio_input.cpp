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

#include "audio_frame_resizer.h"
#include "audio_input.h"
#include "dring/media_const.h"
#include "fileutils.h" // access
#include "manager.h"
#include "media_decoder.h"
#include "resampler.h"
#include "ringbufferpool.h"
#include "smartools.h"

#include <future>
#include <chrono>
#include <memory>

namespace ring {

static constexpr auto MS_PER_PACKET = std::chrono::milliseconds(20);

AudioInput::AudioInput(const std::string& id) :
    id_(id),
    format_(Manager::instance().getRingBufferPool().getInternalAudioFormat()),
    frameSize_(format_.sample_rate * MS_PER_PACKET.count() / 1000),
    resampler_(new Resampler),
    resizer_(new AudioFrameResizer(format_, frameSize_,
       [this](std::shared_ptr<AudioFrame>&& f){ frameResized(std::move(f)); })),
    loop_([] { return true; },
          [this] { process(); },
          [] {})
{
    RING_DBG() << "Creating audio input with id: " << id;
    loop_.start();
}

AudioInput::~AudioInput()
{
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

    // send frame to resizer, frameResized will be called when it can be output
    if (decodingFile_)
        nextFromFile();
    else
        nextFromDevice();
}

void
AudioInput::frameResized(std::shared_ptr<AudioFrame>&& ptr)
{
    std::shared_ptr<AudioFrame> frame = std::move(ptr);
    auto ms = MediaStream("a:local", format_, sent_samples);
    frame->pointer()->pts = sent_samples;
    sent_samples += frame->pointer()->nb_samples;

    notify(std::static_pointer_cast<MediaFrame>(frame));
}

void
AudioInput::nextFromDevice()
{
    auto& mainBuffer = Manager::instance().getRingBufferPool();
    auto bufferFormat = mainBuffer.getInternalAudioFormat();

    // compute number of samples contained in a frame with duration MS_PER_PACKET
    const auto samplesPerPacket = MS_PER_PACKET * bufferFormat.sample_rate;
    const std::size_t samplesToGet = std::chrono::duration_cast<std::chrono::seconds>(samplesPerPacket).count();

    if (mainBuffer.availableForGet(id_) < samplesToGet
        && not mainBuffer.waitForDataAvailable(id_, samplesToGet, MS_PER_PACKET)) {
        return;
    }

    // getData resets the format to internal hardware format, will have to be resampled
    micData_.setFormat(bufferFormat);
    micData_.resize(samplesToGet);
    const auto samples = mainBuffer.getData(micData_, id_);
    if (samples != samplesToGet)
        return;

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

    auto audioFrame = resampled.toAVFrame();
    resizer_->enqueue(std::move(audioFrame));
}

void
AudioInput::nextFromFile()
{
    if (!decoder_)
        return;

    auto frame = std::make_unique<AudioFrame>();
    const auto ret = decoder_->decode(*frame);
    const auto inFmt = AudioFormat((unsigned)frame->pointer()->sample_rate, (unsigned)frame->pointer()->channels, (AVSampleFormat)frame->pointer()->format);

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
            resampler_->resample(frame->pointer(), out.pointer());
            frame->copyFrom(out);
        }
        resizer_->enqueue(std::move(frame));
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
    // sets devOpts_'s sample rate and number of channels
    if (!createDecoder()) {
        RING_WARN() << "Cannot decode audio from file, switching back to default device";
        return initDevice("");
    }
    decodingFile_ = true;
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

bool
AudioInput::createDecoder()
{
    decoder_.reset();
    if (devOpts_.input.empty()) {
        foundDevOpts(devOpts_);
        return false;
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
        return false;
    }

    if (decoder->setupFromAudioData() < 0) {
        RING_ERR() << "Could not setup decoder for '" << devOpts_.input << "'";
        foundDevOpts(devOpts_);
        return false;
    }

    auto ms = decoder->getStream(devOpts_.input);
    devOpts_.channel = ms.nbChannels;
    devOpts_.framerate = ms.sampleRate;
    RING_DBG() << "Created audio decoder: " << ms;

    decoder_ = std::move(decoder);
    foundDevOpts(devOpts_);
    return true;
}

void
AudioInput::setFormat(const AudioFormat& fmt)
{
    std::lock_guard<std::mutex> lk(fmtMutex_);
    format_ = fmt;
    frameSize_ = format_.sample_rate * MS_PER_PACKET.count() / 1000;
    resizer_.reset(new AudioFrameResizer(format_, frameSize_,
       [this](std::shared_ptr<AudioFrame>&& f){ frameResized(std::move(f)); }));
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

} // namespace ring
