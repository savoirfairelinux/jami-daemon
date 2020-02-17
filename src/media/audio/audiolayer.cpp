/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "audiolayer.h"
#include "audio/dcblocker.h"
#include "logger.h"
#include "manager.h"
#include "audio/ringbufferpool.h"
#include "audio/resampler.h"
#include "tonecontrol.h"
#include "client/ring_signal.h"

extern "C" {
#include <speex/speex_echo.h>
}

#include <ctime>
#include <algorithm>

namespace jami {

struct AudioLayer::EchoState
{
    EchoState(AudioFormat format, unsigned frameSize)
        : state(speex_echo_state_init_mc(
                    frameSize, frameSize * 10,
					format.nb_channels, format.nb_channels), &speex_echo_state_destroy)
        , playbackQueue(format, frameSize)
        , recordQueue(format, frameSize) {}

    void putRecorded(std::shared_ptr<AudioFrame>&& in) {
        recordQueue.enqueue(std::move(in));
    }

    void putPlayback(const std::shared_ptr<AudioFrame>& in) {
        auto c = in;
        playbackQueue.enqueue(std::move(c));
    }

    std::shared_ptr<AudioFrame> getRecorded() {
        if (playbackQueue.samples() < playbackQueue.frameSize()
           or recordQueue.samples() < recordQueue.frameSize()) {
            return {};
        }
        while (playbackQueue.samples() > 2 * playbackQueue.frameSize())
            playbackQueue.dequeue();
        while (recordQueue.samples() > 2 * recordQueue.frameSize())
            recordQueue.dequeue();
        auto playback = playbackQueue.dequeue();
        auto record = recordQueue.dequeue();
        if (playback and record) {
            auto ret = std::make_shared<AudioFrame>(record->getFormat(), record->getFrameSize());
            speex_echo_cancellation(state.get(),
                (const int16_t*)record->pointer()->data[0],
                (const int16_t*)playback->pointer()->data[0],
                (int16_t*)ret->pointer()->data[0]);
            return ret;
        }
        return {};
    }

private:
    using SpeexEchoStatePtr = std::unique_ptr<SpeexEchoState, void(*)(SpeexEchoState*)>;
    SpeexEchoStatePtr state;
    AudioFrameResizer playbackQueue;
    AudioFrameResizer recordQueue;
};

AudioLayer::AudioLayer(const AudioPreference &pref)
    : isCaptureMuted_(pref.getCaptureMuted())
    , isPlaybackMuted_(pref.getPlaybackMuted())
    , captureGain_(pref.getVolumemic())
    , playbackGain_(pref.getVolumespkr())
    , mainRingBuffer_(Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
    , audioFormat_(Manager::instance().getRingBufferPool().getInternalAudioFormat())
    , audioInputFormat_(Manager::instance().getRingBufferPool().getInternalAudioFormat())
    , urgentRingBuffer_("urgentRingBuffer_id", SIZEBUF, audioFormat_)
    , resampler_(new Resampler)
    , inputResampler_(new Resampler)
    , lastNotificationTime_()
{
    urgentRingBuffer_.createReadOffset(RingBufferPool::DEFAULT_ID);
}

AudioLayer::~AudioLayer()
{}

void AudioLayer::hardwareFormatAvailable(AudioFormat playback)
{
    JAMI_DBG("Hardware audio format available : %s", playback.toString().c_str());
    audioFormat_ = Manager::instance().hardwareAudioFormatChanged(playback);
    urgentRingBuffer_.setFormat(audioFormat_);

    auto frameSize = audioFormat_.sample_rate / 50;
    echoState_.reset(new EchoState(audioFormat_, frameSize));
}

void AudioLayer::hardwareInputFormatAvailable(AudioFormat capture)
{
    JAMI_DBG("Hardware input audio format available : %s", capture.toString().c_str());
}

void AudioLayer::devicesChanged()
{
    emitSignal<DRing::AudioSignal::DeviceEvent>();
}

void AudioLayer::flushMain()
{
    std::lock_guard<std::mutex> lock(mutex_);
    Manager::instance().getRingBufferPool().flushAllBuffers();
}

void AudioLayer::flushUrgent()
{
    std::lock_guard<std::mutex> lock(mutex_);
    urgentRingBuffer_.flushAll();
}

void AudioLayer::flush()
{
    Manager::instance().getRingBufferPool().flushAllBuffers();
    urgentRingBuffer_.flushAll();
}

void AudioLayer::putUrgent(AudioBuffer& buffer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    urgentRingBuffer_.put(buffer.toAVFrame());
}

// Notify (with a beep) an incoming call when there is already a call in progress
void AudioLayer::notifyIncomingCall()
{
    if (!Manager::instance().incomingCallsWaiting())
        return;

    auto now = std::chrono::system_clock::now();

    // Notify maximum once every 5 seconds
    if ((now - lastNotificationTime_) < std::chrono::seconds(5))
        return;

    lastNotificationTime_ = now;

    // Enable notification only if more than one call
    if (!Manager::instance().hasCurrentCall())
        return;

    Tone tone("440/160", getSampleRate());
    size_t nbSample = tone.getSize();
    AudioBuffer buf(nbSample, AudioFormat::MONO());
    tone.getNext(buf, 1.0);

    /* Put the data in the urgent ring buffer */
    flushUrgent();
    putUrgent(buf);
}

std::shared_ptr<AudioFrame>
AudioLayer::getToRing(AudioFormat format, size_t writableSamples)
{
    ringtoneBuffer_.resize(0);
    if (auto fileToPlay = Manager::instance().getTelephoneFile()) {
        auto fileformat = fileToPlay->getFormat();
        bool resample = format != fileformat;

        size_t readableSamples = resample
                ? (rational<size_t>(writableSamples, format.sample_rate) * (size_t)fileformat.sample_rate).real<size_t>()
                : writableSamples;

        ringtoneBuffer_.setFormat(fileformat);
        ringtoneBuffer_.resize(readableSamples);
        fileToPlay->getNext(ringtoneBuffer_, isRingtoneMuted_ ? 0. : 1.);
        return resampler_->resample(ringtoneBuffer_.toAVFrame(), format);
    }
    return {};
}

std::shared_ptr<AudioFrame>
AudioLayer::getToPlay(AudioFormat format, size_t writableSamples)
{
    notifyIncomingCall();
    auto& bufferPool = Manager::instance().getRingBufferPool();

    if (not playbackQueue_)
        playbackQueue_.reset(new AudioFrameResizer(format, writableSamples));
    else
        playbackQueue_->setFrameSize(writableSamples);

    std::shared_ptr<AudioFrame> playbackBuf {};
    while (!(playbackBuf = playbackQueue_->dequeue())) {
        std::shared_ptr<AudioFrame> resampled;

        if (auto urgentSamples = urgentRingBuffer_.get(RingBufferPool::DEFAULT_ID)) {
            bufferPool.discard(1, RingBufferPool::DEFAULT_ID);
            resampled = resampler_->resample(std::move(urgentSamples),format);
        } else if (auto toneToPlay = Manager::instance().getTelephoneTone()) {
            resampled = resampler_->resample(toneToPlay->getNext(), format);
        } else if (auto buf = bufferPool.getData(RingBufferPool::DEFAULT_ID)) {
            resampled = resampler_->resample(std::move(buf), format);
        } else {
            break;
        }

        if (resampled) {
            if (echoState_)  {
                echoState_->putPlayback(resampled);
            }
        } else
            break;
    }

    return playbackBuf;
}

void
AudioLayer::putRecorded(std::shared_ptr<AudioFrame>&& frame)
{
    //if (isCaptureMuted_)
    //    libav_utils::fillWithSilence(frame->pointer());

    if (echoState_) {
        echoState_->putRecorded(std::move(frame));
        while (auto rec = echoState_->getRecorded())
            mainRingBuffer_->put(std::move(rec));
    } else {
        mainRingBuffer_->put(std::move(frame));
    }
}

} // namespace jami
