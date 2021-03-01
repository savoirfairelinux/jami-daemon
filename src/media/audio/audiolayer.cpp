/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include "echo-cancel/null_echo_canceller.h"

#include <ctime>
#include <algorithm>

namespace jami {

AudioLayer::AudioLayer(const AudioPreference& pref)
    : isCaptureMuted_(pref.getCaptureMuted())
    , isPlaybackMuted_(pref.getPlaybackMuted())
    , captureGain_(pref.getVolumemic())
    , playbackGain_(pref.getVolumespkr())
    , mainRingBuffer_(
          Manager::instance().getRingBufferPool().getRingBuffer(RingBufferPool::DEFAULT_ID))
    , audioFormat_(Manager::instance().getRingBufferPool().getInternalAudioFormat())
    , audioInputFormat_(Manager::instance().getRingBufferPool().getInternalAudioFormat())
    , urgentRingBuffer_("urgentRingBuffer_id", SIZEBUF, audioFormat_)
    , resampler_(new Resampler)
    , lastNotificationTime_()
{
    urgentRingBuffer_.createReadOffset(RingBufferPool::DEFAULT_ID);
}

AudioLayer::~AudioLayer() {}

void
AudioLayer::hardwareFormatAvailable(AudioFormat playback, size_t bufSize)
{
    JAMI_DBG("Hardware audio format available : %s %zu", playback.toString().c_str(), bufSize);
    std::unique_lock<std::mutex> lk(mutex_);
    audioFormat_ = Manager::instance().hardwareAudioFormatChanged(playback);
    urgentRingBuffer_.setFormat(audioFormat_);
    nativeFrameSize_ = bufSize;
}

void
AudioLayer::hardwareInputFormatAvailable(AudioFormat capture)
{
    JAMI_DBG("Hardware input audio format available : %s", capture.toString().c_str());
}

void
AudioLayer::devicesChanged()
{
    emitSignal<DRing::AudioSignal::DeviceEvent>();
}

void
AudioLayer::flushMain()
{
    Manager::instance().getRingBufferPool().flushAllBuffers();
}

void
AudioLayer::flushUrgent()
{
    std::lock_guard<std::mutex> lock(mutex_);
    urgentRingBuffer_.flushAll();
}

void
AudioLayer::flush()
{
    Manager::instance().getRingBufferPool().flushAllBuffers();
    urgentRingBuffer_.flushAll();
}

void
AudioLayer::playbackChanged(bool started)
{
    playbackStarted_ = started;
    checkAEC();
}

void
AudioLayer::recordChanged(bool started)
{
    recordStarted_ = started;
    checkAEC();
}

void
AudioLayer::setHasNativeAEC(bool hasEAC)
{
    hasNativeAEC_ = hasEAC;
    checkAEC();
}

void
AudioLayer::checkAEC()
{
    bool shouldSoftAEC = not hasNativeAEC_ and playbackStarted_ and recordStarted_;

    if (not echoCanceller_ and shouldSoftAEC) {
        JAMI_WARN("Starting AEC");
        echoCanceller_.reset(new NullEchoCanceller(audioFormat_, audioFormat_.sample_rate / 100));
    } else if (echoCanceller_ and not shouldSoftAEC) {
        JAMI_WARN("Stopping AEC");
        echoCanceller_.reset();
    }
}

void
AudioLayer::putUrgent(AudioBuffer& buffer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    urgentRingBuffer_.put(buffer.toAVFrame());
}

// Notify (with a beep) an incoming call when there is already a call in progress
void
AudioLayer::notifyIncomingCall()
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

        size_t readableSamples = resample ? (rational<size_t>(writableSamples, format.sample_rate)
                                             * (size_t) fileformat.sample_rate)
                                                .real<size_t>()
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
            resampled = resampler_->resample(std::move(urgentSamples), format);
        } else if (auto toneToPlay = Manager::instance().getTelephoneTone()) {
            resampled = resampler_->resample(toneToPlay->getNext(), format);
        } else if (auto buf = bufferPool.getData(RingBufferPool::DEFAULT_ID)) {
            resampled = resampler_->resample(std::move(buf), format);
        } else {
            break;
        }

        if (resampled) {
            if (echoCanceller_) {
                echoCanceller_->putPlayback(resampled);
            }
            playbackQueue_->enqueue(std::move(resampled));
        } else
            break;
    }

    return playbackBuf;
}

void
AudioLayer::putRecorded(std::shared_ptr<AudioFrame>&& frame)
{
    if (echoCanceller_) {
        echoCanceller_->putRecorded(std::move(frame));
        while (auto rec = echoCanceller_->getProcessed())
            mainRingBuffer_->put(std::move(rec));
    } else {
        mainRingBuffer_->put(std::move(frame));
    }
}

} // namespace jami
