/*
 *  Copyright (C) 2004, 2005, 2006, 2009Savoir-Faire Linux inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 *  Portions (c) Dominic Mazzoni (Audacity)
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "ringbuffer.h"
#include "../global.h"

#define MIN_BUFFER_SIZE	1280

// Create  a ring buffer with 'size' bytes
RingBuffer::RingBuffer (int size) : mStart (0), mEnd (0)
        , mBufferSize (size > MIN_BUFFER_SIZE ? size : MIN_BUFFER_SIZE)
        , mBuffer (NULL)
{
    mBuffer = new unsigned char[mBufferSize];
    assert (mBuffer != NULL);

    createReadPointer();
    storeReadPointer(mStart);
}

// Free memory on object deletion
RingBuffer::~RingBuffer()
{
    delete[] mBuffer;
    mBuffer = NULL;

    removeReadPointer();
}

void
RingBuffer::flush (CallID call_id)
{
    storeReadPointer(0, call_id);
    mEnd = 0;
}

int
RingBuffer::putLen()
{
    if(_readpointer.size() > 1)
    {
	mStart = getSmallestReadPointer();
    }
    else
    {
        mStart = getReadPointer();
    }

    return (mEnd + mBufferSize - mStart) % mBufferSize;
}

int
RingBuffer::getLen(CallID call_id)
{

    mStart = getReadPointer(call_id);
    return (mEnd + mBufferSize - mStart) % mBufferSize;

}

void
RingBuffer::debug()
{
    _debug ("Start=%d; End=%d; BufferSize=%d\n", mStart, mEnd, mBufferSize);
}

int
RingBuffer::getReadPointer(CallID call_id)
{

    ReadPointer::iterator iter = _readpointer.find(call_id);
    if (iter == _readpointer.end())
	return NULL;
    else
        return iter->second;
    
}

int
RingBuffer::getSmallestReadPointer()
{
    int smallest = mBufferSize;

    ReadPointer::iterator iter;
    for( iter = _readpointer.begin(); iter != _readpointer.end(); iter++)
    {
	if((iter->first != "default_id") && (iter->second < smallest))
	    smallest = iter->second;
    }

    return smallest;
}

void
RingBuffer::storeReadPointer(int pointer_value, CallID call_id)
{

    ReadPointer::iterator iter = _readpointer.find(call_id);
    if(iter != _readpointer.end())
    {	
	iter->second = pointer_value;
    }
    else{
	_debug("Cannot find \"%s\" readPointer\n", call_id.c_str());
    }

}


void
RingBuffer::createReadPointer(CallID call_id)
{

    _readpointer[call_id] = 0;

}


void
RingBuffer::removeReadPointer(CallID call_id)
{

    _readpointer.erase(call_id);

}


int
RingBuffer::getNbReadPointer()
{

    return _readpointer.size();

}

//
// For the writer only:
//
int
RingBuffer::AvailForPut()
{
    // Always keep 4 bytes safe (?)
    return (mBufferSize-4) - putLen();
}

// This one puts some data inside the ring buffer.
// Change the volume if it's not 100
int
RingBuffer::Put (void* buffer, int toCopy, unsigned short volume)
{
    samplePtr src;
    int block;
    int copied;
    int pos;
    int len = putLen();

    if (toCopy > (mBufferSize-4) - len)
        toCopy = (mBufferSize-4) - len;

    src = (samplePtr) buffer;


    copied = 0;

    pos = mEnd;

    while (toCopy) {
        block = toCopy;

        // Wrap block around ring ?

        if (block > (mBufferSize - pos)) {
            // Fill in to the end of the buffer
            block = mBufferSize - pos;
        }

        // Gain adjustment (when Mic vol. is changed)
        if (volume != 100) {
            SFLDataFormat* start = (SFLDataFormat*) src;
            int nbSample = block / sizeof (SFLDataFormat);

            for (int i=0; i<nbSample; i++) {
                start[i] = start[i] * volume / 100;
            }
        }

        // bcopy(src, dest, len)
        //fprintf(stderr, "has %d put %d\t", len, block);
        bcopy (src, mBuffer + pos, block);

        src += block;

        pos = (pos + block) % mBufferSize;

        toCopy -= block;

        copied += block;
    }

    mEnd = pos;

    // How many items copied.
    return copied;
}

//
// For the reader only:
//

int
RingBuffer::AvailForGet(CallID call_id)
{
    // Used space
    return getLen(call_id);
}

// Get will move 'toCopy' bytes from the internal FIFO to 'buffer'
int
RingBuffer::Get (void *buffer, int toCopy, unsigned short volume, CallID call_id)
{
    samplePtr dest;
    int block;
    int copied;
    int len = getLen(call_id);

    if (toCopy > len)
        toCopy = len;

    dest = (samplePtr) buffer;

    copied = 0;

    mStart = getReadPointer(call_id);

    //fprintf(stderr, "G");
    while (toCopy) {
        block = toCopy;

        if (block > (mBufferSize - mStart)) {
            block = mBufferSize - mStart;
        }

        if (volume!=100) {
            SFLDataFormat* start = (SFLDataFormat*) (mBuffer + mStart);
            int nbSample = block / sizeof (SFLDataFormat);

            for (int i=0; i<nbSample; i++) {
                start[i] = start[i] * volume / 100;
            }
        }

        // bcopy(src, dest, len)
        bcopy (mBuffer + mStart, dest, block);

        dest += block;

        mStart = (mStart + block) % mBufferSize;

        toCopy -= block;

        copied += block;
    }

    storeReadPointer(mStart, call_id);

    return copied;
}

// Used to discard some bytes.
int
RingBuffer::Discard (int toDiscard, CallID call_id)
{
    
    int len = getLen(call_id);

    mStart = getReadPointer(call_id);

    if (toDiscard > len)
        toDiscard = len;

    mStart = (mStart + toDiscard) % mBufferSize;

    storeReadPointer(mStart, call_id);

    return toDiscard;
}
