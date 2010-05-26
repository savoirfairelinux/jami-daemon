/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#ifndef __MAIN_BUFFER__
#define __MAIN_BUFFER__

#include <map>
#include <set>
#include <cc++/thread.h> // for ost::Mutex
#include <string>

#include "../global.h"
#include "../call.h"
#include "ringbuffer.h"



typedef std::map<CallID, RingBuffer*> RingBufferMap;

typedef std::set<CallID> CallIDSet;

typedef std::map<CallID, CallIDSet*> CallIDMap;

class MainBuffer {

public:

        MainBuffer();

        ~MainBuffer();

	void setInternalSamplingRate(int sr);

	int getInternalSamplingRate() {return _internalSamplingRate;}

	CallIDSet* getCallIDSet(CallID call_id);

	bool createCallIDSet(CallID set_id);

	bool removeCallIDSet(CallID set_id);

	void addCallIDtoSet(CallID set_id, CallID call_id);

	void removeCallIDfromSet(CallID set_id, CallID call_id);

	RingBuffer* createRingBuffer(CallID call_id);

	bool removeRingBuffer(CallID call_id);

	void bindCallID(CallID call_id1, CallID call_id2 = default_id);

	void unBindCallID(CallID call_id1, CallID call_id2 = default_id);

	void unBindAll(CallID call_id);

	int putData(void *buffer, int toCopy, unsigned short volume = 100, CallID call_id = default_id);

	int getData(void *buffer, int toCopy, unsigned short volume = 100, CallID call_id = default_id);

	int availForPut(CallID call_id = default_id);

	int availForGet(CallID call_id = default_id);

	int discard(int toDiscard, CallID call_id = default_id);

	void flush(CallID call_id = default_id);

	void flushAllBuffers();

	void flushDefault();

	void stateInfo();

    private:

	RingBuffer* getRingBuffer(CallID call_id);

	int getDataByID(void *buffer, int toCopy, unsigned short volume, CallID call_id, CallID reader_id);

	int availForGetByID(CallID call_id, CallID reader_id);

	int discardByID(int toDiscard, CallID call_id, CallID reader_id);

	void flushByID(CallID call_id, CallID reader_id);

	RingBufferMap _ringBufferMap;

	CallIDMap _callIDMap;

	SFLDataFormat* mixBuffer;

	// ost::Mutex _mutex;

	int _internalSamplingRate;

    public:

	friend class MainBufferTest;
};

#endif
