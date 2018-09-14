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

#include <future>
#include <chrono>
#include "socket_pair.h"
#include "audio/ringbufferpool.h"
#include "manager.h"
#include "smartools.h"

namespace ring {

AudioInput::AudioInput(const std::string& id) :
    resampler_(new Resampler),
    id_(id),
    targetFormat_(AudioFormat::STEREO())
{}

AudioInput::AudioInput(const std::string& id, AudioFormat target) :
    resampler_(new Resampler),
    id_(id),
    targetFormat_(target)
{}

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

std::unique_ptr<AudioFrame>
AudioInput::getNextFrame()
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

    std::unique_ptr<AudioFrame> audio;
    audio.reset(new AudioFrame);
    auto frame = audio->pointer();
    if (bufferFormat.sample_rate != targetFormat_.sample_rate) {
        frame->format = AV_SAMPLE_FMT_S16;
        frame->sample_rate = targetFormat_.sample_rate;
        frame->channels = targetFormat_.nb_channels;
        frame->channel_layout = av_get_default_channel_layout(targetFormat_.nb_channels);
        resampler_->resample(micData_.toAVFrame(), frame);
    } else {
        frame = micData_.toAVFrame();
    }

    auto ms = MediaStream("a:local", targetFormat_);
    frame->pts = getNextTimestamp(sent_samples, ms.sampleRate, static_cast<rational<int64_t>>(ms.timeBase));
    sent_samples += frame->nb_samples;
    ms.firstTimestamp = frame->pts;

    {
        auto rec = recorder_.lock();
        if (rec && rec->isRecording()) {
            rec->recordData(frame, ms);
        }
    }

    return audio;
}

void
AudioInput::setMuted(bool isMuted)
{
    muteState_ = isMuted;
}

std::shared_future<DeviceParams>
AudioInput::switchInput(const std::string& resource)
{
    // TODO not implemented yet
    return {};
}

void
AudioInput::initRecorder(const std::shared_ptr<MediaRecorder>& rec)
{
    rec->incrementExpectedStreams(1);
    recorder_ = rec;
}

} // namespace ring
