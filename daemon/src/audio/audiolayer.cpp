/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

AudioLayer::AudioLayer()
    : isStarted_(false)
    , urgentRingBuffer_(SIZEBUF, Call::DEFAULT_ID)
    , audioSampleRate_(Manager::instance().getMainBuffer()->getInternalSamplingRate())
    , mutex_()
    , audioPref(Manager::instance().audioPreference)
    , converter_(new SamplerateConverter(audioSampleRate_))
    , lastNotificationTime_(0)
{
    urgentRingBuffer_.createReadPointer();
}


AudioLayer::~AudioLayer()
{
    delete converter_;
}

void AudioLayer::flushMain(void)
{
    ost::MutexLock guard(mutex_);
    // should pass call id
    Manager::instance().getMainBuffer()->flushAllBuffers();
}

void AudioLayer::flushUrgent(void)
{
    ost::MutexLock guard(mutex_);
    urgentRingBuffer_.flushAll();
}

void AudioLayer::putUrgent(void* buffer, int toCopy)
{
    ost::MutexLock guard(mutex_);
    urgentRingBuffer_.Put(buffer, toCopy);
}

// Notify (with a beep) an incoming call when there is already a call in progress
void AudioLayer::notifyincomingCall()
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
    Manager::instance().audioLayerMutexLock();
    flushUrgent();
    putUrgent(buf, sizeof buf);
    Manager::instance().audioLayerMutexUnlock();
}

