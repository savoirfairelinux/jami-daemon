/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include <cstdlib>
#include <cstring>
#include <utility> // for std::pair

#include "ringbuffer.h"
#include "global.h"

// corespond to 106 ms (about 5 rtp packets)
#define MIN_BUFFER_SIZE	1280

int RingBuffer::count_rb = 0;

// Create  a ring buffer with 'size' bytes
RingBuffer::RingBuffer(int size, const std::string &call_id) : mEnd(0)
    , mBufferSize(size > MIN_BUFFER_SIZE ? size : MIN_BUFFER_SIZE)
    , mBuffer(NULL)
    , buffer_id(call_id)
{
    mBuffer = new unsigned char[mBufferSize];
    count_rb++;
}

RingBuffer::~RingBuffer()
{
    delete[] mBuffer;
}

void
RingBuffer::flush(const std::string &call_id)
{
    storeReadPointer(mEnd, call_id);
}


void
RingBuffer::flushAll()
{
    ReadPointer::iterator iter;

    for (iter = _readpointer.begin(); iter != _readpointer.end(); ++iter)
        iter->second = mEnd;
}

int
RingBuffer::putLen()
{
    int mStart = (_readpointer.size() >= 1) ? getSmallestReadPointer() : 0;
    return (mEnd + mBufferSize - mStart) % mBufferSize;
}

int
RingBuffer::getLen(const std::string &call_id)
{
    return (mEnd + mBufferSize - getReadPointer(call_id)) % mBufferSize;
}

void
RingBuffer::debug()
{
    _debug("Start=%d; End=%d; BufferSize=%d", getSmallestReadPointer(), mEnd, mBufferSize);
}

int
RingBuffer::getReadPointer(const std::string &call_id)
{
    if (getNbReadPointer() == 0)
        return 0;

    ReadPointer::iterator iter = _readpointer.find(call_id);
    return (iter != _readpointer.end()) ? iter->second : 0;
}

int
RingBuffer::getSmallestReadPointer()
{
    if (getNbReadPointer() == 0)
        return 0;

    int smallest = mBufferSize;

    ReadPointer::iterator iter;

    for (iter = _readpointer.begin(); iter != _readpointer.end(); ++iter)
        if (iter->second < smallest)
            smallest = iter->second;

    return smallest;
}

void
RingBuffer::storeReadPointer(int pointer_value, const std::string &call_id)
{
    ReadPointer::iterator iter = _readpointer.find(call_id);

    if (iter != _readpointer.end()) {
        iter->second = pointer_value;
    } else {
        _debug("storeReadPointer: Cannot find \"%s\" readPointer in \"%s\" ringbuffer", call_id.c_str(), buffer_id.c_str());
    }
}


void
RingBuffer::createReadPointer(const std::string &call_id)
{
    if (!hasThisReadPointer(call_id))
        _readpointer.insert(std::pair<std::string, int> (call_id, mEnd));
}


void
RingBuffer::removeReadPointer(const std::string &call_id)
{
    ReadPointer::iterator iter = _readpointer.find(call_id);

    if (iter != _readpointer.end())
        _readpointer.erase(iter);
}


bool
RingBuffer::hasThisReadPointer(const std::string &call_id)
{
    return _readpointer.find(call_id) != _readpointer.end();
}


int
RingBuffer::getNbReadPointer()
{
    return _readpointer.size();
}

//
// For the writer only:
//

// This one puts some data inside the ring buffer.
void
RingBuffer::Put(void* buffer, int toCopy)
{
    int len = putLen();

    if (toCopy > mBufferSize - len)
        toCopy = mBufferSize - len;

    unsigned char *src = (unsigned char *) buffer;

    int pos = mEnd;

    while (toCopy) {
        int block = toCopy;

        if (block > mBufferSize - pos) // Wrap block around ring ?
            block = mBufferSize - pos; // Fill in to the end of the buffer

        memcpy(mBuffer + pos, src, block);
        src += block;
        pos = (pos + block) % mBufferSize;
        toCopy -= block;
    }

    mEnd = pos;
}

//
// For the reader only:
//

int
RingBuffer::AvailForGet(const std::string &call_id)
{
    // Used space

    return getLen(call_id);
}

// Get will move 'toCopy' bytes from the internal FIFO to 'buffer'
int
RingBuffer::Get(void *buffer, int toCopy, const std::string &call_id)
{
    if (getNbReadPointer() == 0)
        return 0;

    if (!hasThisReadPointer(call_id))
        return 0;

    int len = getLen(call_id);

    if (toCopy > len)
        toCopy = len;

    int copied = toCopy;

    unsigned char *dest = (unsigned char *) buffer;
    int mStart = getReadPointer(call_id);

    while (toCopy) {
        int block = toCopy;

        if (block > mBufferSize - mStart)
            block = mBufferSize - mStart;

        memcpy(dest, mBuffer + mStart, block);
        dest += block;
        mStart = (mStart + block) % mBufferSize;
        toCopy -= block;
    }

    storeReadPointer(mStart, call_id);
    return copied;
}

int
RingBuffer::Discard(int toDiscard, const std::string &call_id)
{
    int len = getLen(call_id);

    if (toDiscard > len)
        toDiscard = len;

    int mStart = (getReadPointer(call_id) + toDiscard) % mBufferSize;

    storeReadPointer(mStart, call_id);

    return toDiscard;
}
