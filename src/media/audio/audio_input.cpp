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
#include "audio/resampler.h"
#include "manager.h"
#include "smartools.h"

namespace ring {

static constexpr auto MS_PER_PACKET = std::chrono::milliseconds(20);

AudioInput::AudioInput(const std::string& id) :
    id_(id),
    format_(Manager::instance().getRingBufferPool().getInternalAudioFormat()),
    resampler_(new Resampler),
    loop_([] { return true; },
          [this] { process(); },
          [] {})
{
    loop_.start();
}

AudioInput::~AudioInput()
{
    if (auto rec = recorder_.lock())
        rec->stopRecording();
    loop_.join();
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

void
AudioInput::process()
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

    AudioBuffer resampled;
    resampled.setFormat(format_);
    if (bufferFormat != format_) {
        resampler_->resample(micData_, resampled);
    } else {
        resampled = micData_;
    }

    auto audioFrame = resampled.toAVFrame();
    auto frame = audioFrame->pointer();
    auto ms = MediaStream("a:local", format_);
    frame->pts = getNextTimestamp(sent_samples, ms.sampleRate, static_cast<rational<int64_t>>(ms.timeBase));
    sent_samples += frame->nb_samples;

    {
        auto rec = recorder_.lock();
        if (rec && rec->isRecording()) {
            rec->recordData(frame, ms);
        }
    }

    std::shared_ptr<AudioFrame> sharedFrame = std::move(audioFrame);
    notify(sharedFrame);
}

void
AudioInput::setFormat(const AudioFormat& fmt)
{
    format_ = fmt;
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
