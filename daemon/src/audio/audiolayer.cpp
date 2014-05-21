/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "audiolayer.h"
#include "audio/dcblocker.h"
#include "logger.h"
#include "manager.h"

#include <ctime>

AudioLayer::AudioLayer(const AudioPreference &pref)
    : isCaptureMuted_(pref.getCaptureMuted())
    , isPlaybackMuted_(pref.getPlaybackMuted())
    , captureGain_(pref.getVolumemic())
    , playbackGain_(pref.getVolumespkr())
    , isStarted_(false)
    , audioFormat_(Manager::instance().getMainBuffer().getInternalAudioFormat())
    , urgentRingBuffer_(SIZEBUF, MainBuffer::DEFAULT_ID, audioFormat_)
    , mutex_()
    , dcblocker_()
    , resampler_(audioFormat_.sample_rate)
    , lastNotificationTime_(0)
{
    urgentRingBuffer_.createReadOffset(MainBuffer::DEFAULT_ID);
}

AudioLayer::~AudioLayer()
{}

void AudioLayer::hardwareFormatAvailable(AudioFormat playback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    DEBUG("hardwareFormatAvailable : %s", playback.toString().c_str());
    urgentRingBuffer_.setFormat(playback);
    resampler_.setFormat(playback);
    Manager::instance().hardwareAudioFormatChanged(playback);
}

void AudioLayer::flushMain()
{
    std::lock_guard<std::mutex> lock(mutex_);
    // should pass call id
    Manager::instance().getMainBuffer().flushAllBuffers();
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
    AudioBuffer buf(nbSample, AudioFormat::MONO);
    tone.getNext(buf, 1.0);

    /* Put the data in the urgent ring buffer */
    flushUrgent();
    putUrgent(buf);
}
