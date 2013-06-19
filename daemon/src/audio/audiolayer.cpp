/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <ctime>
#include "audiolayer.h"
#include "audio/dcblocker.h"
#include "manager.h"
#include "scoped_lock.h"

unsigned int AudioLayer::captureGain_ = 100;
unsigned int AudioLayer::playbackGain_ = 100;

AudioLayer::AudioLayer()
    : isStarted_(false)
    , playbackMode_(NONE)
    , urgentRingBuffer_(SIZEBUF, MainBuffer::DEFAULT_ID)
    , sampleRate_(Manager::instance().getMainBuffer().getInternalSamplingRate())
    , mutex_()
    , dcblocker_()
    , converter_(sampleRate_)
    , lastNotificationTime_(0)
{
    pthread_mutex_init(&mutex_, NULL);
    urgentRingBuffer_.createReadPointer(MainBuffer::DEFAULT_ID);
}

AudioLayer::~AudioLayer()
{
    pthread_mutex_destroy(&mutex_);
}

void AudioLayer::flushMain()
{
    sfl::ScopedLock guard(mutex_);
    // should pass call id
    Manager::instance().getMainBuffer().flushAllBuffers();
}

void AudioLayer::flushUrgent()
{
    sfl::ScopedLock guard(mutex_);
    urgentRingBuffer_.flushAll();
}

void AudioLayer::putUrgent(void* buffer, int toCopy)
{
    sfl::ScopedLock guard(mutex_);
    urgentRingBuffer_.put(buffer, toCopy);
}

void AudioLayer::applyGain(SFLDataFormat *src , int samples, int gain)
{
    if (gain != 100)
        for (int i = 0 ; i < samples; i++)
            src[i] = src[i] * gain* 0.01;
}

// Notify (with a beep) an incoming call when there is already a call in progress
void AudioLayer::notifyIncomingCall()
{
    if (!Manager::instance().incomingCallWaiting())
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
    SFLDataFormat buf[nbSample];
    tone.getNext(buf, nbSample);

    /* Put the data in the urgent ring buffer */
    flushUrgent();
    putUrgent(buf, sizeof buf);
}

bool AudioLayer::audioBufferFillWithZeros(AudioBuffer &buffer) {

    memset(buffer.data(), 0, buffer.size());

    return true;
}

bool AudioLayer::audioPlaybackFillWithToneOrRingtone(AudioBuffer &buffer) {
    AudioLoop *tone = Manager::instance().getTelephoneTone();
    AudioLoop *file_tone = Manager::instance().getTelephoneFile();

    // In case of a dtmf, the pointers will be set to NULL once the dtmf length is
    // reached. For this reason we need to fill audio buffer with zeros if pointer is NULL
    if (tone) {
        tone->getNext(buffer.data(), buffer.length(), getPlaybackGain());
    }
    else if (file_tone) {
        file_tone->getNext(buffer.data(), buffer.length(), getPlaybackGain());
    }
    else {
        audioBufferFillWithZeros(buffer);
    }

    return true;
}

bool AudioLayer::audioPlaybackFillWithUrgent(AudioBuffer &buffer, size_t bytesToGet) {
    // Urgent data (dtmf, incoming call signal) come first.
    bytesToGet = std::min(bytesToGet, buffer.size());
    const size_t samplesToGet = bytesToGet / sizeof(SFLDataFormat);
    urgentRingBuffer_.get(buffer.data(), bytesToGet, MainBuffer::DEFAULT_ID);
    // AudioLayer::applyGain(buffer.size, samplesToGet, getPlaybackGain());

    // Consume the regular one as well (same amount of bytes)
    Manager::instance().getMainBuffer().discard(bytesToGet, MainBuffer::DEFAULT_ID);

    return true;
}

bool AudioLayer::audioPlaybackFillWithVoice(AudioBuffer &buffer, size_t bytesAvail) {
    const size_t bytesToCpy = buffer.size();

    const size_t mainBufferSampleRate = Manager::instance().getMainBuffer().getInternalSamplingRate();
    const bool resample = sampleRate_ != mainBufferSampleRate;

    if(bytesAvail == 0)
        return false;

    double resampleFactor = 1.0;
    size_t maxNbBytesToGet = bytesToCpy;
    if (resample) {
        resampleFactor = mainBufferSampleRate / static_cast<double>(sampleRate_);
        maxNbBytesToGet = bytesToCpy * resampleFactor;
    }

    size_t bytesToGet = std::min(maxNbBytesToGet, bytesAvail);

    const size_t samplesToGet = bytesToGet / sizeof(SFLDataFormat);
    AudioBuffer out(samplesToGet);
    SFLDataFormat * out_ptr = NULL;
    if(resample)
        out_ptr = &(*out.data());
    else
        out_ptr = &(*buffer.data());

    Manager::instance().getMainBuffer().getData(out_ptr, bytesToGet, MainBuffer::DEFAULT_ID);
    // AudioLayer::applyGain(out_ptr, samplesToGet, getPlaybackGain());

    if (resample) {
        SFLDataFormat * const rsmpl_out_ptr = buffer.data();
        const size_t outSamples = samplesToGet * resampleFactor;
        const size_t outBytes = outSamples * sizeof(SFLDataFormat);
        converter_.resample(out_ptr, rsmpl_out_ptr, outSamples,
                mainBufferSampleRate, sampleRate_, samplesToGet);
    }

    return true;
}

bool AudioLayer::audioPlaybackFillBuffer(AudioBuffer &buffer) {
    // Looks if there's any voice audio from rtp to be played
    MainBuffer &mbuffer = Manager::instance().getMainBuffer();
    size_t bytesToGet = mbuffer.availableForGet(MainBuffer::DEFAULT_ID);
    size_t urgentBytesToGet = urgentRingBuffer_.availableForGet(MainBuffer::DEFAULT_ID);


    PlaybackMode mode = getPlaybackMode();

    bool bufferFilled = false;

    switch(mode) {
    case NONE:
    case TONE:
    case RINGTONE:
    case URGENT: {
        if (urgentBytesToGet > 0)
            bufferFilled = audioPlaybackFillWithUrgent(buffer, urgentBytesToGet);
        else
            bufferFilled = audioPlaybackFillWithToneOrRingtone(buffer);
        }
        break;
    case VOICE: {
        if(bytesToGet > 0)
            bufferFilled = audioPlaybackFillWithVoice(buffer, bytesToGet);
        else
            bufferFilled = audioBufferFillWithZeros(buffer);
        }
        break;
    case ZEROS:
    default:
        bufferFilled = audioBufferFillWithZeros(buffer);
    }

    if(!bufferFilled)
        printf("Error buffer not filled in audio playback\n");

    return bufferFilled;
}

// #define RECORD_TOMAIN_TODISK
#ifdef RECORD_TOMAIN_TODISK
#include <fstream>
std::ofstream opensl_tomainbuffer("/data/data/com.savoirfairelinux.sflphone/opensl_tomain.raw", std::ofstream::out | std::ofstream::binary);
#endif

void AudioLayer::audioCaptureFillBuffer(AudioBuffer &buffer) {
    const int toGetBytes = buffer.size();
    const int toGetSamples = buffer.length();

    MainBuffer &mbuffer = Manager::instance().getMainBuffer();

    const int mainBufferSampleRate = mbuffer.getInternalSamplingRate();
    const bool resample = mbuffer.getInternalSamplingRate() != sampleRate_;

    SFLDataFormat *in_ptr = buffer.data();
    AudioLayer::applyGain(in_ptr, toGetSamples, getCaptureGain());

    if (resample) {
        int outSamples = toGetSamples * (static_cast<double>(sampleRate_) / mainBufferSampleRate);
        AudioBuffer rsmpl_out(outSamples);
        SFLDataFormat * const rsmpl_out_ptr = rsmpl_out.data();
        converter_.resample(in_ptr, rsmpl_out_ptr,
                rsmpl_out.length(), mainBufferSampleRate, sampleRate_,
                toGetSamples);
        dcblocker_.process(rsmpl_out_ptr, rsmpl_out_ptr, outSamples);
        mbuffer.putData(rsmpl_out_ptr, rsmpl_out.size(), MainBuffer::DEFAULT_ID);
    } else {
        dcblocker_.process(in_ptr, in_ptr, toGetSamples);
#ifdef RECORD_TOMAIN_TODISK
        opensl_tomainbuffer.write((char const *)in_ptr, toGetBytes/); 
#endif
        mbuffer.putData(in_ptr, toGetBytes, MainBuffer::DEFAULT_ID);
    }
}
