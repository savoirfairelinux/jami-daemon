/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author : Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 * 
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

#ifndef __MAIN_BUFFER__
#define __MAIN_BUFFER__

#include <map>
#include <set>
#include <cc++/thread.h> // for ost::Mutex

#include "../global.h"
#include "../call.h"
#include "ringbuffer.h"

typedef std::map<CallID, RingBuffer*> RingBufferMap;

typedef std::map<CallID, CallID> CallIDMap;

#define default_id "default_id"

class MainBuffer {

    public:

        MainBuffer();

        ~MainBuffer();

	int putData(void *buffer, int toCopy, unsigned short volume = 100, CallID call_id = default_id);

	int getData(void *buffer, int toCopy, unsigned short volume = 100, CallID call_id = default_id);

	int getDataByID(void *buffer, int toCopy, unsigned short volume = 100, CallID call_id = default_id);

	int availForPut(CallID call_id = default_id);

	// int availForGet(CallID call_id = default_id);

	// int discard(int toDiscard, CallID call_id = default_id);

	// void flush(CallID call_id = default_id);

    private:

	RingBuffer* createRingBuffer(CallID call_id);

	RingBuffer* getRingBuffer(CallID call_id);

	bool removeRingBuffer(CallID call_id);

	RingBufferMap _ringBufferMap;

	CallIDMap _callIDMap;

	ost::Mutex _mutex;

    public:

	friend class MainBufferTest;
};

#endif
