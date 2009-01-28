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

#ifndef __EVENT_THREAD_H__
#define __EVENT_THREAD_H__

#include <cc++/thread.h>

class VoIPLink;

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


#endif // __EVENT_THREAD_H__
