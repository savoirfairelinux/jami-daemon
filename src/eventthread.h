/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
 * General thread to listen events continuously
 */
class EventThread : public ost::Thread {
public:
  /**
   * Build a thread that call getEvents 
   */
	EventThread (VoIPLink*);
	~EventThread (void);
	
	virtual void 	 run ();
	virtual void	 stop();
	virtual void	 startLoop();
	bool		 isStopped();

private:
  /** VoIPLink is the object being called by getEvents() method  */
	VoIPLink*	_linkthread;
	bool		stopIt;
};

#endif // __EVENT_THREAD_H__
