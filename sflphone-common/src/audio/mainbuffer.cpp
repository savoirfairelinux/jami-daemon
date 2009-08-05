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

#include "mainbuffer.h"

MainBuffer::MainBuffer()
{
    createRingBuffer(default_id);
    createCallIDSet(default_id);

    mixBuffer = new SFLDataFormat[3000];
}


MainBuffer::~MainBuffer()
{
    removeRingBuffer(default_id);
    removeCallIDSet(default_id);
}

CallIDSet* MainBuffer::getCallIDSet(CallID call_id)
{

    // _debug("MainBuffer::getCallIDSet\n");

    CallIDMap::iterator iter = _callIDMap.find(call_id);
    if (iter == _callIDMap.end())
    {
	// _debug("CallIDSet with ID: \"%s\" doesn't exist! \n", call_id.c_str());
	return NULL;
    }
    else
	return iter->second;

}

bool MainBuffer::createCallIDSet(CallID set_id)
{

    _callIDMap[set_id] = new CallIDSet;

    return true;

}

bool MainBuffer::removeCallIDSet(CallID set_id)
{

    CallIDSet* callid_set = getCallIDSet(set_id);
    delete callid_set;
    callid_set = NULL;

    if (_callIDMap.erase(set_id) != 0)
	return true;
    else
	return false;

}

void MainBuffer::addCallIDtoSet(CallID set_id, CallID call_id)
{

    CallIDSet* callid_set = getCallIDSet(set_id);
    callid_set->insert(call_id);

}

void MainBuffer::removeCallIDfromSet(CallID set_id, CallID call_id)
{
    CallIDSet* callid_set = getCallIDSet(set_id);
    if(callid_set != NULL)
        callid_set->erase(call_id);
}


RingBuffer* MainBuffer::getRingBuffer(CallID call_id)
{
    
    RingBufferMap::iterator iter = _ringBufferMap.find(call_id);
    if (iter == _ringBufferMap.end())
    {
	// _debug("ringBuffer with ID: \"%s\" doesn't exist! \n", call_id.c_str());
	return NULL;
    }
    else
	return iter->second;
}


RingBuffer* MainBuffer::createRingBuffer(CallID call_id)
{
    // addCallIDtoSet(default_id, call_id);
    // addCallIDtoSet(call_id, default_id);

    RingBuffer* newRingBuffer = new RingBuffer(SIZEBUF);

    _ringBufferMap[call_id] = newRingBuffer;

    return newRingBuffer;
}


bool MainBuffer::removeRingBuffer(CallID call_id)
{
    // removeCallIDFromSet(default_id, call_id);

    // _callIDMap.erase(default_id);
    // _callIDMap.erase(call_id);

    RingBuffer* ring_buffer = getRingBuffer(call_id);
    if(ring_buffer != NULL)
    {
        delete ring_buffer;
        ring_buffer = NULL;

        if (_ringBufferMap.erase(call_id) != 0)
            return true;
        else
	    return false;
    }
    else
	return true;
}


void MainBuffer::bindCallID(CallID call_id1, CallID call_id2)
{

    RingBuffer* ring_buffer = getRingBuffer(call_id1);
    CallIDSet* callid_set = getCallIDSet(call_id1);

    if(callid_set == NULL)
	createCallIDSet(call_id1);

    if(ring_buffer == NULL)
	createRingBuffer(call_id1);

 
    addCallIDtoSet(call_id1, call_id2);
    addCallIDtoSet(call_id2, call_id1);

    getRingBuffer(call_id1)->createReadPointer(call_id2);
    getRingBuffer(call_id2)->createReadPointer(call_id1);

}


void MainBuffer::unBindCallID(CallID call_id1, CallID call_id2)
{
    // _debug("MainBuffer::unBindCallID\n");

    RingBuffer* ringbuffer;

    ringbuffer = getRingBuffer(call_id1);
    if(ringbuffer != NULL)
	ringbuffer->removeReadPointer(call_id2);
    
    ringbuffer = getRingBuffer(call_id2);
    if(ringbuffer != NULL)
	ringbuffer->removeReadPointer(call_id1);

    removeCallIDfromSet(call_id1, call_id2);
    removeCallIDfromSet(call_id2, call_id1);

    ringbuffer = getRingBuffer(call_id1);
    if(ringbuffer != NULL)
    {
        if(getRingBuffer(call_id1)->getNbReadPointer() < 1)
        {

	    removeRingBuffer(call_id1);
	    removeCallIDSet(call_id1);

        }
    }

}


int MainBuffer::putData(void *buffer, int toCopy, unsigned short volume, CallID call_id)
{

    RingBuffer* ring_buffer = getRingBuffer(call_id);

    if (ring_buffer == NULL)
    {
	// _debug("Input RingBuffer ID: \"%s\" does not exist!\n", call_id.c_str());
	return 0;
    }

    int a;

    ost::MutexLock guard (_mutex);
    a = ring_buffer->AvailForPut();

    if (a >= toCopy) {
        return ring_buffer->Put (buffer, toCopy, volume);
    } else {
        // _debug ("Chopping sound, Ouch! RingBuffer full ?\n");
        return ring_buffer->Put (buffer, a, volume);
    }

    return 0;

}

int MainBuffer::availForPut(CallID call_id)
{

    return getRingBuffer(call_id)->AvailForPut();

}


int MainBuffer::getData(void *buffer, int toCopy, unsigned short volume, CallID call_id)
{

    // _debug("MainBuffer::getData\n");

    CallIDSet* callid_set = getCallIDSet(call_id);

    if(callid_set->empty())
    {
	// _debug("CallIDSet with ID: \"%s\" is empty!\n", call_id.c_str());
	return 0;
    }
    
    if (callid_set->size() == 1)
    {
	CallIDSet::iterator iter_id = callid_set->begin();
	return getDataByID(buffer, toCopy, volume, *iter_id, call_id);
    }
    else
    {

	for (int k = 0; k < toCopy; k++)
	{
	    ((SFLDataFormat*)(buffer))[k] = 0;
	}

	// _debug("CallIDSet with ID: \"%s\" is a conference!\n", call_id.c_str());
	CallIDSet::iterator iter_id = callid_set->begin();
	for(iter_id = callid_set->begin(); iter_id != callid_set->end(); iter_id++)
	{
	    getDataByID(mixBuffer, toCopy, volume, *iter_id, call_id);
	    for (int k = 0; k < toCopy; k++)
	    {
		((SFLDataFormat*)(buffer))[k] += mixBuffer[k];
	    }
	}
	
	return toCopy;
    }
}


int MainBuffer::getDataByID(void *buffer, int toCopy, unsigned short volume, CallID call_id, CallID reader_id)
{

    RingBuffer* ring_buffer = getRingBuffer(call_id);

    if (ring_buffer == NULL)
    {
	// _debug("Output RingBuffer ID: \"%s\" does not exist!\n", call_id.c_str());
	return 0;
    }

    int a;

    ost::MutexLock guard (_mutex);
    a = ring_buffer->AvailForGet(call_id);

    if (a >= toCopy) {
        return ring_buffer->Get (buffer, toCopy, volume, reader_id);
    } else {
        // _debug ("RingBuffer is quite empty\n");
        return ring_buffer->Get (buffer, a, volume, reader_id);
    }

    return 0;

}


int MainBuffer::availForGet(CallID call_id)
{
    _debug("MainBuffer::availForGet\n");

    CallIDSet* callid_set = getCallIDSet(call_id);

    if (callid_set->empty())
    {
	// _debug("CallIDSet with ID: \"%s\" is empty!\n", call_id.c_str());
	return 0;
    }

    if (callid_set->size() == 1)
    {
	CallIDSet::iterator iter_id = callid_set->begin();
	return availForGetByID(*iter_id, call_id);
    }
    else
    {
	// _debug("CallIDSet with ID: \"%s\" is a conference!\n", call_id.c_str());
	int avail_bytes = 99999;
	int nb_bytes;
	CallIDSet::iterator iter_id = callid_set->begin();
	for(iter_id = callid_set->begin(); iter_id != callid_set->end(); iter_id++)
	{
	    nb_bytes = availForGetByID(*iter_id, call_id);
	    if (nb_bytes < avail_bytes)
		avail_bytes = nb_bytes;
	}
	return avail_bytes;
    }

}


int MainBuffer::availForGetByID(CallID call_id, CallID reader_id)
{

    return getRingBuffer(call_id)->AvailForGet(reader_id);

}


int MainBuffer::discard(int toDiscard, CallID call_id)
{
    // _debug("MainBuffer::discard\n");

    CallIDSet* callid_set = getCallIDSet(call_id);

    if(callid_set->empty())
    {
	// _debug("CallIDSet with ID: \"%s\" is empty!\n", call_id.c_str());
	return 0;
    }


    if (callid_set->size() == 1)
    {
	CallIDSet::iterator iter_id = callid_set->begin();
	// _debug("Discard Data in \"%s\" RingBuffer for \"%s\" ReaderPointer\n",(*iter_id).c_str(),call_id.c_str());
	return discardByID(toDiscard, *iter_id, call_id);
    }
    else
    {
	// _debug("CallIDSet with ID: \"%s\" is a conference!\n", call_id.c_str());
	CallIDSet::iterator iter_id = callid_set->begin();
	for(iter_id = callid_set->begin(); iter_id != callid_set->end(); iter_id++)
	{
	    discardByID(toDiscard, *iter_id, call_id);
	}
	return toDiscard;
    }

}


int MainBuffer::discardByID(int toDiscard, CallID call_id, CallID reader_id)
{

    return getRingBuffer(call_id)->Discard(toDiscard, reader_id);

}



void MainBuffer::flush(CallID call_id)
{

    // _debug("MainBuffer::flush\n");

    CallIDSet* callid_set = getCallIDSet(call_id);

    if(callid_set->empty())
    {
	// _debug("CallIDSet with ID: \"%s\" is empty!\n", call_id.c_str());
    }

    if (callid_set->size() == 1)
    {
	CallIDSet::iterator iter_id = callid_set->begin();
	flushByID(*iter_id, call_id);
    }
    else
    {
	// _debug("CallIDSet with ID: \"%s\" is a conference!\n", call_id.c_str());
	CallIDSet::iterator iter_id = callid_set->begin();
	for(iter_id = callid_set->begin(); iter_id != callid_set->end(); iter_id++)
	{
	    flushByID(*iter_id, call_id);
	}
    }

}

void MainBuffer::flushDefault()
{

    flushByID(default_id);

}


void MainBuffer::flushByID(CallID call_id, CallID reader_id)
{

    getRingBuffer(call_id)->flush(reader_id);
}
