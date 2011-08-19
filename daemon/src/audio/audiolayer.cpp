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

#include "audiolayer.h"
#include "audioprocessing.h"
#include "audio/dcblocker.h"
#include "manager.h"
#include <cc++/numbers.h>

AudioLayer::AudioLayer (ManagerImpl* manager , int type)
    : layerType_ (type)
    , isStarted_ (false)
    , manager_ (manager)
    , urgentRingBuffer_ (SIZEBUF, Call::DEFAULT_ID)
    , mainBuffer_ (0)
    , recorder_ (0)
    , indexIn_ (0)
    , indexOut_ (0)
    , indexRing_ (0)
    , audioSampleRate_ (0)
    , frameSize_ (0)
    , inChannel_ (1)
    , outChannel_ (1)
    , errorMessage_ (0)
    , mutex_ ()
    , dcblocker_ (0)
    , audiofilter_ (0)
    , noiseSuppressState_ (false)
    , countNotificationTime_ (0)
      , time_ (new ost::Time)
{}


AudioLayer::~AudioLayer ()
{
    delete time_;
    delete audiofilter_;
    delete dcblocker_;
}

void AudioLayer::flushMain (void)
{
    ost::MutexLock guard (mutex_);
    // should pass call id
    getMainBuffer()->flushAllBuffers();
}

void AudioLayer::flushUrgent (void)
{
    ost::MutexLock guard (mutex_);
    urgentRingBuffer_.flushAll();
}

void AudioLayer::putUrgent (void* buffer, int toCopy)
{
    ost::MutexLock guard (mutex_);
    urgentRingBuffer_.Put (buffer, toCopy);
}

void AudioLayer::notifyincomingCall()
{
    // Notify (with a beep) an incoming call when there is already a call
    if (Manager::instance().incomingCallWaiting()) {
        countNotificationTime_ += time_->getSecond();
        int countTimeModulo = countNotificationTime_ % 5000;

        if ((countTimeModulo - countNotificationTime_) < 0)
            Manager::instance().notificationIncomingCall();

        countNotificationTime_ = countTimeModulo;
    }
}

