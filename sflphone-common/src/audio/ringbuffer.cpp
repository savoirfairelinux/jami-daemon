/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "ringbuffer.h"
#include "global.h"

#define MIN_BUFFER_SIZE	1280

int RingBuffer::count_rb = 0;

// Create  a ring buffer with 'size' bytes
RingBuffer::RingBuffer (int size, CallID call_id) : mEnd (0)
        , mBufferSize (size > MIN_BUFFER_SIZE ? size : MIN_BUFFER_SIZE)
        , mBuffer (NULL)
        , buffer_id (call_id)
{
    mBuffer = new unsigned char[mBufferSize];
    assert (mBuffer != NULL);

    count_rb++;
}

// Free memory on object deletion
RingBuffer::~RingBuffer()
{
    delete[] mBuffer;
    mBuffer = NULL;
}

void
RingBuffer::flush (CallID call_id)
{

    storeReadPointer (mEnd, call_id);
}


void
RingBuffer::flushAll ()
{


    ReadPointer::iterator iter_pointer = _readpointer.begin();

    while (iter_pointer != _readpointer.end()) {

        iter_pointer->second = mEnd;

        iter_pointer++;
    }
}

int
RingBuffer::putLen()
{
    int mStart;

    if (_readpointer.size() >= 1) {
        mStart = getSmallestReadPointer();
    } else {
        mStart = 0;
    }

    int length = (mEnd + mBufferSize - mStart) % mBufferSize;

    return length;
}

int
RingBuffer::getLen (CallID call_id)
{

    int mStart = getReadPointer (call_id);

    int length = (mEnd + mBufferSize - mStart) % mBufferSize;

    
    // _debug("    *RingBuffer::getLen: buffer_id %s, call_id %s, mStart %i, mEnd %i, length %i, buffersie %i", buffer_id.c_str(), call_id.c_str(), mStart, mEnd, length, mBufferSize);
    return length;

}

void
RingBuffer::debug()
{
    int mStart = getSmallestReadPointer();

    _debug ("Start=%d; End=%d; BufferSize=%d", mStart, mEnd, mBufferSize);
}

int
RingBuffer::getReadPointer (CallID call_id)
{

    if (getNbReadPointer() == 0)
        return 0;

    ReadPointer::iterator iter = _readpointer.find (call_id);

    if (iter == _readpointer.end()) {
        return 0;
    } else {
        return iter->second;
    }

}

int
RingBuffer::getSmallestReadPointer()
{
    if (getNbReadPointer() == 0)
        return 0;

    int smallest = mBufferSize;

    ReadPointer::iterator iter = _readpointer.begin();

    while (iter != _readpointer.end()) {
        if (iter->second < smallest)
            smallest = iter->second;

        iter++;
    }

    return smallest;
}

void
RingBuffer::storeReadPointer (int pointer_value, CallID call_id)
{

    ReadPointer::iterator iter = _readpointer.find (call_id);

    if (iter != _readpointer.end()) {
        iter->second = pointer_value;
    } else {
        _debug ("storeReadPointer: Cannot find \"%s\" readPointer in \"%s\" ringbuffer", call_id.c_str(), buffer_id.c_str());
    }

}


void
RingBuffer::createReadPointer (CallID call_id)
{

    _readpointer.insert (pair<CallID, int> (call_id, mEnd));

}


void
RingBuffer::removeReadPointer (CallID call_id)
{


    _readpointer.erase (call_id);


}


bool
RingBuffer::hasThisReadPointer (CallID call_id)
{
    ReadPointer::iterator iter = _readpointer.find (call_id);

    if (iter == _readpointer.end()) {
        return false;
    } else {
        return true;
    }
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

    return mBufferSize - putLen();
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


    if (toCopy > mBufferSize - len)
        toCopy = mBufferSize - len;

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
RingBuffer::AvailForGet (CallID call_id)
{
    // Used space

    return getLen(call_id);
}

// Get will move 'toCopy' bytes from the internal FIFO to 'buffer'
int
RingBuffer::Get (void *buffer, int toCopy, unsigned short volume, CallID call_id)
{

    if (getNbReadPointer() == 0)
        return 0;

    if (!hasThisReadPointer (call_id))
        return 0;

    samplePtr dest;

    int block;

    int copied;

    int len = getLen (call_id);


    if (toCopy > len)
        toCopy = len;

    dest = (samplePtr) buffer;

    copied = 0;

    int mStart = getReadPointer (call_id);

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

    storeReadPointer (mStart, call_id);

    return copied;
}

// Used to discard some bytes.
int
RingBuffer::Discard (int toDiscard, CallID call_id)
{

    int len = getLen (call_id);

    int mStart = getReadPointer (call_id);

    if (toDiscard > len)
        toDiscard = len;

    mStart = (mStart + toDiscard) % mBufferSize;

    storeReadPointer (mStart, call_id);

    return toDiscard;
}
