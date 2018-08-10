/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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

namespace ring { namespace audio {

AudioInput::AudioInput(const std::string& id) :
    id_(id),
    loop_([] {return true;}, // setup()
          [this] {return process();},
          [this] {return cleanup();})
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
    auto ringBuffer = mainBuffer.getRingBuffer(id_);
    auto bufferFormat = ringBuffer->getFormat();

    // compute number of bytes contained in a frame with duration msPerPacket_
    auto frameDuration = std::chrono::duration_cast<std::chrono::seconds>(msPerPacket_).count();
    const std::size_t bytesToGet = frameDuration * bufferFormat.sample_rate;

    if (ringBuffer->availableForGet(id_) < bytesToGet
        && not ringBuffer->waitForDataAvailable(id_, bytesToGet)) {
        return;
    }

    // get data
    micData_.setFormat(bufferFormat);
    micData_.resize(bytesToGet);
    const auto samples = ringBuffer->get(micData_, id_);
    if (samples != bytesToGet)
        return;

    if (muteState_) // audio is muted, set samples to 0
        micData_.reset();

    // record frame
    AVFrame* frame = micData_.toAVFrame();
    auto ms = MediaStream("a:local", micData_.getFormat());
    frame->pts = getNextTimestamp(sent_samples, ms.sampleRate, static_cast<rational<int64_t>>(ms.timeBase));
    sent_samples += frame->nb_samples;

    {
        auto rec = recorder_.lock();
        if (rec && rec->isRecording()) {
            rec->recordData(frame, ms);
        } else {
            RING_DBG() << "could not record data (received data but recorder is invalid or not recording)";
        }
    }
}

void
AudioInput::cleanup()
{
    micData_.clear();
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

}} // namespace ring::audio
