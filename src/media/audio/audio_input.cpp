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
#include "audio/ringbufferpool.h"
#include "manager.h"
#include "media_device.h"
#include "smartools.h"
#include "socket_pair.h"
#include "dring/media_const.h"
#include "fileutils.h"

#include <future>
#include <chrono>

namespace ring {

AudioInput::AudioInput(const std::string& id) :
    id_(id),
    targetFormat_(AudioFormat::STEREO())
{
    resampler_.reset(new Resampler);
}

AudioInput::AudioInput(const std::string& id, AudioFormat target) :
    id_(id),
    targetFormat_(target)
{
    resampler_.reset(new Resampler);
}

AudioInput::~AudioInput()
{
    if (auto rec = recorder_.lock())
        rec->stopRecording();
}

// seq: frame number for video, sent samples audio
// sampleFreq: fps for video, sample rate for audio
// clock: stream time base (packetization interval times)
// FIXME duplicate code from media encoder
int64_t
getNextTimestamp(int64_t seq, rational<int64_t> sampleFreq, rational<int64_t> clock)
{
    return (seq / (sampleFreq * clock)).real<int64_t>();
}

AVFrame*
AudioInput::getNextFrame()
{
    foundDevOpts(devOpts_);
    if (switchPending_.exchange(false)) {
        std::string input = devOpts_.input.empty() ? "default" : devOpts_.input;
        RING_DBG() << "Switching audio input to '" << input << "'";
        // TODO emit stop and start signals
    }

    AVFrame* frame = nullptr;
    if (currentResource_.find(DRing::Media::VideoProtocolPrefix::FILE) != 0) {
        frame = getNextFromInput();
    } else {
        AudioFrame aframe;
        if (getNextFromFile(aframe)) {
            // make sure decoded frame data is not freed when aframe is not longer in scope
            frame = av_frame_clone(aframe.pointer());
        }
    }

    if (!frame)
        return nullptr;

    auto ms = MediaStream("a:local", targetFormat_);
    frame->pts = getNextTimestamp(sampleCount, ms.sampleRate, static_cast<rational<int64_t>>(ms.timeBase));
    sampleCount += frame->nb_samples;

    {
        auto rec = recorder_.lock();
        if (rec && rec->isRecording()) {
            rec->recordData(frame, ms);
        }
    }

    return frame;
}

AVFrame*
AudioInput::getNextFromInput()
{
    auto& mainBuffer = Manager::instance().getRingBufferPool();
    auto bufferFormat = mainBuffer.getInternalAudioFormat();

    // compute number of bytes contained in a frame with duration msPerPacket_
    const std::size_t samplesToGet = std::chrono::duration_cast<std::chrono::seconds>(msPerPacket_ * bufferFormat.sample_rate).count();

    if (mainBuffer.availableForGet(id_) < samplesToGet
        && not mainBuffer.waitForDataAvailable(id_, samplesToGet, msPerPacket_)) {
        return nullptr;
    }

    // get data
    micData_.setFormat(bufferFormat);
    micData_.resize(samplesToGet);
    const auto samples = mainBuffer.getData(micData_, id_);
    if (samples != samplesToGet)
        return nullptr;

    if (muteState_) // audio is muted, set samples to 0
        micData_.reset();

    AVFrame* frame;
    if (bufferFormat.sample_rate != targetFormat_.sample_rate) {
        if (!resampler_)
            resampler_.reset(new Resampler);
        frame = av_frame_alloc();
        frame->format = AV_SAMPLE_FMT_S16;
        frame->sample_rate = targetFormat_.sample_rate;
        frame->channels = targetFormat_.nb_channels;
        frame->channel_layout = av_get_default_channel_layout(targetFormat_.nb_channels);
        resampler_->resample(micData_.toAVFrame(), frame);
    } else {
        frame = micData_.toAVFrame();
    }

    return frame;
}

bool
AudioInput::getNextFromFile(AudioFrame& frame)
{
    if (!decoder_)
        return false;

    const auto ret = decoder_->decode(frame);
    switch (ret) {
        case MediaDecoder::Status::ReadError:
            return false;
        case MediaDecoder::Status::DecodeError:
            RING_WARN() << "Failed to decode frame";
            return false;
        case MediaDecoder::Status::RestartRequired:
        case MediaDecoder::Status::EOFError:
            createDecoder();
            return static_cast<bool>(decoder_);
        case MediaDecoder::Status::FrameFinished:
            return true;
        case MediaDecoder::Status::Success:
        default:
            return true;
    }
}

bool
AudioInput::initInput(const std::string& input)
{
    devOpts_ = {};
    devOpts_.input = input;
    const AudioFormat& fmt = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    devOpts_.framerate = fmt.sample_rate;
    devOpts_.channel = fmt.nb_channels;

    return true;
}

bool
AudioInput::initFile(const std::string& path)
{
    if (access(path.c_str(), R_OK) != 0) {
        RING_ERR() << "File '" << path << "' unavailable";
        return false;
    }

    devOpts_ = {};
    devOpts_.input = path;
    devOpts_.loop = "1";
    createDecoder();
    return false;
}

std::shared_future<DeviceParams>
AudioInput::switchInput(const std::string& resource)
{
    if (resource == currentResource_)
        return futureDevOpts_;

    RING_DBG() << "Switching audio source to match '" << resource << "'";

    if (switchPending_) {
        RING_ERR() << "Audio switch already requested";
        return {};
    }

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

    if (prefix == DRing::Media::VideoProtocolPrefix::FILE) {
        ready = initFile(suffix);
    } else {
        ready = initInput(suffix);
    }

    if (ready) {
        foundDevOpts(devOpts_);
    }

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
    switchPending_ = false;

    if (devOpts_.input.empty()) {
        foundDevOpts(devOpts_);
        return;
    }

    auto decoder = std::make_unique<MediaDecoder>();
    // TODO setInterruptCallback

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
