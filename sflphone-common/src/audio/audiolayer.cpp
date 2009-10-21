/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 */

#include "audiolayer.h"

void AudioLayer::flushMain (void)
{
    ost::MutexLock guard (_mutex);

    // should pass call id 
    _mainBuffer.flushAllBuffers();
}

void AudioLayer::flushUrgent (void)
{
    ost::MutexLock guard (_mutex);
    _urgentRingBuffer.flushAll();
}

void AudioLayer::flushMic (void)
{
    ost::MutexLock guard (_mutex);
    _mainBuffer.flushDefault();
}

int AudioLayer::putUrgent (void* buffer, int toCopy)
{
    int a;

    ost::MutexLock guard (_mutex);
    a = _urgentRingBuffer.AvailForPut();

    if (a >= toCopy) {
        return _urgentRingBuffer.Put (buffer, toCopy, _defaultVolume);
    } else {
        return _urgentRingBuffer.Put (buffer, a, _defaultVolume);
    }

    return 0;
}

int AudioLayer::putMain (void *buffer, int toCopy, CallID call_id)
{
    int a;

    ost::MutexLock guard (_mutex);
    a = _mainBuffer.availForPut(call_id);

    if (a >= toCopy) {
        return _mainBuffer.putData (buffer, toCopy, _defaultVolume, call_id);
    } else {
        _debug ("Chopping sound, Ouch! RingBuffer full ?\n");
        return _mainBuffer.putData (buffer, a, _defaultVolume, call_id);
    }

    return 0;
}


