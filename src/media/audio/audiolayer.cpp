/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
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

#include <ctime>

namespace ring {

AudioLayer::AudioLayer(const AudioPreference &pref)
    : isCaptureMuted_(pref.getCaptureMuted())
    , isPlaybackMuted_(pref.getPlaybackMuted())
    , captureGain_(pref.getVolumemic())
    , playbackGain_(pref.getVolumespkr())
    , audioFormat_(Manager::instance().getRingBufferPool().getInternalAudioFormat())
    , audioInputFormat_(Manager::instance().getRingBufferPool().getInternalAudioFormat())
    , urgentRingBuffer_("urgentRingBuffer_id", SIZEBUF, audioFormat_)
    , resampler_(new Resampler{audioFormat_.sample_rate})
    , inputResampler_(new Resampler{audioInputFormat_.sample_rate})
    , lastNotificationTime_(0)
{
    urgentRingBuffer_.createReadOffset(RingBufferPool::DEFAULT_ID);
}

AudioLayer::~AudioLayer()
{}

void AudioLayer::hardwareFormatAvailable(AudioFormat playback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    RING_DBG("Hardware audio format available : %s", playback.toString().c_str());
    audioFormat_ = Manager::instance().hardwareAudioFormatChanged(playback);
    urgentRingBuffer_.setFormat(audioFormat_);
    resampler_->setFormat(audioFormat_);
}

void AudioLayer::hardwareInputFormatAvailable(AudioFormat capture)
{
    inputResampler_->setFormat(capture);
}

void AudioLayer::flushMain()
{
    std::lock_guard<std::mutex> lock(mutex_);
    // should pass call id
    Manager::instance().getRingBufferPool().flushAllBuffers();
}

void AudioLayer::flushUrgent()
{
    std::lock_guard<std::mutex> lock(mutex_);
    urgentRingBuffer_.flushAll();
}

void AudioLayer::putUrgent(AudioBuffer& buffer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    urgentRingBuffer_.put(buffer);
}

// Notify (with a beep) an incoming call when there is already a call in progress
void AudioLayer::notifyIncomingCall()
{
    if (!Manager::instance().incomingCallsWaiting())
        return;

    time_t now = time(NULL);

    // Notify maximum once every 5 seconds
    if (difftime(now, lastNotificationTime_) < 5)
        return;

    lastNotificationTime_ = now;

    // Enable notification only if more than one call
    if (!Manager::instance().hasCurrentCall())
        return;

    Tone tone("440/160", getSampleRate());
    unsigned int nbSample = tone.getSize();
    AudioBuffer buf(nbSample, AudioFormat::MONO());
    tone.getNext(buf, 1.0);

    /* Put the data in the urgent ring buffer */
    flushUrgent();
    putUrgent(buf);
}


AudioBuffer AudioLayer::getToPlay(AudioFormat format, size_t writableSamples)
{
    /*size_t sample_size = format.getBytesPerFrame();
    if (writableBytes == 0)
        return {};

    const size_t writableSamples = writableBytes / sample_size;*/

    notifyIncomingCall();

    size_t urgentSamples = urgentRingBuffer_.availableForGet(RingBufferPool::DEFAULT_ID);
    //size_t urgentBytes = urgentSamples * sample_size;

    if (urgentSamples > writableSamples) {
        urgentSamples = writableSamples;
        //urgentBytes = urgentSamples * sample_size;
    }

    //AudioSample* data = nullptr;
    AudioBuffer playbackBuffer_;

    if (urgentSamples) {
        playbackBuffer_.setFormat(format);
        playbackBuffer_.resize(urgentSamples);
        //pa_stream_begin_write(playback_->stream(), (void**)&data, &urgentBytes);
        urgentRingBuffer_.get(playbackBuffer_, RingBufferPool::DEFAULT_ID); // retrive only the first sample_spec->channels channels
        playbackBuffer_.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
        //playbackBuffer_.interleave(data);
        //pa_stream_write(playback_->stream(), data, urgentBytes, nullptr, 0, PA_SEEK_RELATIVE);
        // Consume the regular one as well (same amount of samples)
        Manager::instance().getRingBufferPool().discard(urgentSamples, RingBufferPool::DEFAULT_ID);
        return std::move(playbackBuffer_);
    }

    // FIXME: not thread safe! we only lock the mutex when we get the
    // pointer, we have no guarantee that it will stay safe to use
    AudioLoop *toneToPlay = Manager::instance().getTelephoneTone();
    if (toneToPlay) {
        //if (playback_->isReady()) {
            //pa_stream_begin_write(playback_->stream(), (void**)&data, &writableBytes);
            playbackBuffer_.setFormat(format);
            playbackBuffer_.resize(writableSamples);
            toneToPlay->getNext(playbackBuffer_, playbackGain_); // retrive only n_channels
            //playbackBuffer_.interleave(data);
            //pa_stream_write(playback_->stream(), data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);
        //}

        return std::move(playbackBuffer_);
    }

    flushUrgent(); // flush remaining samples in _urgentRingBuffer

    size_t availSamples = Manager::instance().getRingBufferPool().availableForGet(RingBufferPool::DEFAULT_ID);

    if (availSamples == 0) {
        /*pa_stream_begin_write(playback_->stream(), (void**)&data, &writableBytes);
        memset(data, 0, writableBytes);
        pa_stream_write(playback_->stream(), data, writableBytes, nullptr, 0, PA_SEEK_RELATIVE);*/
        //playbackBuffer_.resize(writableSamples);
        return std::move(playbackBuffer_);
    }

    // how many samples we want to read from the buffer
    size_t readableSamples = writableSamples;

    double resampleFactor = 1.;

    AudioFormat mainBufferAudioFormat = Manager::instance().getRingBufferPool().getInternalAudioFormat();
    bool resample = audioFormat_.sample_rate != mainBufferAudioFormat.sample_rate;

    if (resample) {
        resampleFactor = (double) audioFormat_.sample_rate / mainBufferAudioFormat.sample_rate;
        readableSamples = (double) readableSamples / resampleFactor;
    }

    readableSamples = std::min(readableSamples, availSamples);
    size_t nResampled = (double) readableSamples * resampleFactor;
    //size_t resampledBytes =  nResampled * sample_size;

    //pa_stream_begin_write(playback_->stream(), (void**)&data, &resampledBytes);

    playbackBuffer_.setFormat(mainBufferAudioFormat);
    playbackBuffer_.resize(readableSamples);
    Manager::instance().getRingBufferPool().getData(playbackBuffer_, RingBufferPool::DEFAULT_ID);
    playbackBuffer_.setChannelNum(format.nb_channels, true);

    if (resample) {
        AudioBuffer rsmpl_out(nResampled, format);
        resampler_->resample(playbackBuffer_, rsmpl_out);
        rsmpl_out.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
        return std::move(rsmpl_out);
        /*rsmpl_out.interleave(data);
        pa_stream_write(playback_->stream(), data, resampledBytes, nullptr, 0, PA_SEEK_RELATIVE);*/
    } else {
        playbackBuffer_.applyGain(isPlaybackMuted_ ? 0.0 : playbackGain_);
        /*playbackBuffer_.interleave(data);
        pa_stream_write(playback_->stream(), data, resampledBytes, nullptr, 0, PA_SEEK_RELATIVE);*/
        return std::move(playbackBuffer_);
    }
}


} // namespace ring
