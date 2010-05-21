/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#include "eventthread.h"
#include "voiplink.h"
#include "audio/alsa/alsalayer.h"

/********************************** Voiplink thread *************************************/
EventThread::EventThread (VoIPLink *link)
        : Thread(), _linkthread (link)
{
    setCancel (cancelDeferred);
}


/**
 * Reimplementation of run()
 */
void EventThread::run (void)
{
    while (!testCancel()) {
        _linkthread->getEvent();
    }
}

/********************************************************************************************/

AudioThread::AudioThread (AlsaLayer *alsa)
        : Thread(), _alsa (alsa)
{
    setCancel (cancelDeferred);
}

/**
 * Reimplementation of run()
 */
void AudioThread::run (void)
{
    while (!testCancel()) {
        _alsa->audioCallback();
        Thread::sleep (20);
    }
}

