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
    : _layerType (type)
    , _isStarted(false)
    , _manager (manager)
    , _urgentRingBuffer (SIZEBUF, Call::DEFAULT_ID)
    , _mainBuffer(0)
    , _recorder(0)
    , _indexIn (0)
    , _indexOut (0)
    , _indexRing(0)
    , _audioSampleRate (0)
    , _frameSize (0)
    , _inChannel (1)
    , _outChannel (1)
    , _errorMessage (0)
    , _mutex ()
    , _dcblocker(0)
    , _audiofilter(0)
    , _noisesuppressstate(false)
    , _countNotificationTime(0)
      , _time (new ost::Time)
{}


AudioLayer::~AudioLayer ()
{
    delete _time;
    delete _audiofilter;
    delete _dcblocker;
}

void AudioLayer::flushMain (void)
{
    ost::MutexLock guard (_mutex);
    // should pass call id
    getMainBuffer()->flushAllBuffers();
}

void AudioLayer::flushUrgent (void)
{
    ost::MutexLock guard (_mutex);
    _urgentRingBuffer.flushAll();
}

void AudioLayer::putUrgent (void* buffer, int toCopy)
{
    ost::MutexLock guard (_mutex);
    _urgentRingBuffer.Put (buffer, toCopy);
}

void AudioLayer::notifyincomingCall()
{
    // Notify (with a beep) an incoming call when there is already a call
    if (Manager::instance().incomingCallWaiting()) {
        _countNotificationTime += _time->getSecond();
        int countTimeModulo = _countNotificationTime % 5000;

        if ((countTimeModulo - _countNotificationTime) < 0)
            Manager::instance().notificationIncomingCall();

        _countNotificationTime = countTimeModulo;
    }
}

