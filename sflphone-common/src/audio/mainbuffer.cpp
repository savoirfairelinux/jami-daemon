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
}


MainBuffer::~MainBuffer()
{
    removeRingBuffer(default_id);
}


RingBuffer* MainBuffer::getRingBuffer(CallID call_id)
{
    
    RingBufferMap::iterator iter = _ringBufferMap.find(call_id);
    if (iter == _ringBufferMap.end())
	return NULL;
    else
	return iter->second;
}


RingBuffer* MainBuffer::createRingBuffer(CallID call_id)
{

    RingBuffer* newRingBuffer = new RingBuffer(SIZEBUF);

    _ringBufferMap[call_id] = newRingBuffer;

    return newRingBuffer;
}


bool MainBuffer::removeRingBuffer(CallID call_id)
{

    RingBuffer* ring_buffer = getRingBuffer(call_id);
    delete ring_buffer;
    ring_buffer = NULL;

    if (_ringBufferMap.erase(call_id) != 0)
        return true;
    else
	return false;
}


int MainBuffer::putData(void *buffer, int toCopy, unsigned short volume, CallID call_id)
{

    RingBuffer* ring_buffer = getRingBuffer(call_id);

    int a;

    ost::MutexLock guard (_mutex);
    a = ring_buffer->AvailForPut();

    if (a >= toCopy) {
        return ring_buffer->Put (buffer, toCopy, volume);
    } else {
        _debug ("Chopping sound, Ouch! RingBuffer full ?\n");
        return ring_buffer->Put (buffer, a, volume);
    }

    return 0;

}


int MainBuffer::getData(void *buffer, int toCopy, unsigned short volume, CallID call_id)
{

    RingBuffer* ring_buffer = getRingBuffer(call_id);

    int a;

    ost::MutexLock guard (_mutex);
    a = ring_buffer->AvailForGet();

    if (a >= toCopy) {
        return ring_buffer->Get (buffer, toCopy, volume);
    } else {
        _debug ("RingBuffer is quite empty\n");
        return ring_buffer->Get (buffer, a, volume);
    }

    return 0;

}
