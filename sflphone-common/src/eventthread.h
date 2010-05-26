/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#ifndef __EVENT_THREAD_H__
#define __EVENT_THREAD_H__

#include <cc++/thread.h>

class VoIPLink;
class AlsaLayer;

/**
 * @file eventthread.h
 * @brief General thread to listen events continuously
 */

class EventThread : public ost::Thread {

    public:
        /**
         * Thread constructor 
         */
        EventThread (VoIPLink* link);        
        
        ~EventThread (void){
            terminate();
        }
        
        virtual void run () ;
        
    private:
        EventThread(const EventThread& rh); // copy constructor
        EventThread& operator=(const EventThread& rh);  // assignment operator	

        /** VoIPLink is the object being called by getEvents() method  */
        VoIPLink*	_linkthread;
};

class AudioThread : public ost::Thread {

    public:
        AudioThread (AlsaLayer *alsa);

        ~AudioThread (void){
            terminate();
        }

        virtual void run (void);

    private:
        AudioThread (const AudioThread& at);
        AudioThread& operator=(const AudioThread& at);

        AlsaLayer* _alsa;
};

#endif // __EVENT_THREAD_H__
