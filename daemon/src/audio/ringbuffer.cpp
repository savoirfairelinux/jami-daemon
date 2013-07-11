/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#include "logger.h"
#include "ringbuffer.h"

namespace {
// corresponds to 106 ms (about 5 rtp packets)
const size_t MIN_BUFFER_SIZE = 1280;
}

// Create  a ring buffer with 'size' bytes
RingBuffer::RingBuffer(size_t size, const std::string &call_id) :
    endPos_(0)
    , bufferSize_(size > MIN_BUFFER_SIZE ? size : MIN_BUFFER_SIZE)
    , buffer_(bufferSize_)
    , readpointers_()
    , buffer_id_(call_id)
{}

void
RingBuffer::flush(const std::string &call_id)
{
    storeReadPointer(endPos_, call_id);
}


void
RingBuffer::flushAll()
{
    ReadPointer::iterator iter;

    for (iter = readpointers_.begin(); iter != readpointers_.end(); ++iter)
        iter->second = endPos_;
}

size_t
RingBuffer::putLength() const
{
    const size_t startPos = (not readpointers_.empty()) ? getSmallestReadPointer() : 0;
    return (endPos_ + bufferSize_ - startPos) % bufferSize_;
}

size_t RingBuffer::getLength(const std::string &call_id) const
{
    return (endPos_ + bufferSize_ - getReadPointer(call_id)) % bufferSize_;
}

void
RingBuffer::debug()
{
    DEBUG("Start=%d; End=%d; BufferSize=%d", getSmallestReadPointer(), endPos_, bufferSize_);
}

size_t RingBuffer::getReadPointer(const std::string &call_id) const
{
    if (hasNoReadPointers())
        return 0;

    ReadPointer::const_iterator iter = readpointers_.find(call_id);
    return (iter != readpointers_.end()) ? iter->second : 0;
}

size_t
RingBuffer::getSmallestReadPointer() const
{
    if (hasNoReadPointers())
        return 0;

    size_t smallest = bufferSize_;

    ReadPointer::const_iterator iter;

    for (iter = readpointers_.begin(); iter != readpointers_.end(); ++iter)
        if (iter->second < smallest)
            smallest = iter->second;

    return smallest;
}

void
RingBuffer::storeReadPointer(size_t pointer_value, const std::string &call_id)
{
    ReadPointer::iterator iter = readpointers_.find(call_id);

    if (iter != readpointers_.end())
        iter->second = pointer_value;
    else
        DEBUG("Cannot find \"%s\" readPointer in \"%s\" ringbuffer", call_id.c_str(), buffer_id_.c_str());
}


void
RingBuffer::createReadPointer(const std::string &call_id)
{
    if (!hasThisReadPointer(call_id))
        readpointers_.insert(std::pair<std::string, int> (call_id, endPos_));
}


void
RingBuffer::removeReadPointer(const std::string &call_id)
{
    ReadPointer::iterator iter = readpointers_.find(call_id);

    if (iter != readpointers_.end())
        readpointers_.erase(iter);
}


bool
RingBuffer::hasThisReadPointer(const std::string &call_id) const
{
    return readpointers_.find(call_id) != readpointers_.end();
}


bool RingBuffer::hasNoReadPointers() const
{
    return readpointers_.empty();
}

//
// For the writer only:
//

// This one puts some data inside the ring buffer.
void
RingBuffer::put(void* buffer, size_t toCopy)
{
    const size_t len = putLength();

    if (toCopy > bufferSize_ - len)
        toCopy = bufferSize_ - len;

    unsigned char *src = static_cast<unsigned char *>(buffer);

    size_t pos = endPos_;

    while (toCopy) {
        size_t block = toCopy;

        if (block > bufferSize_ - pos) // Wrap block around ring ?
            block = bufferSize_ - pos; // Fill in to the end of the buffer

        memcpy(&(*buffer_.begin()) + pos, src, block);
        src += block;
        pos = (pos + block) % bufferSize_;
        toCopy -= block;
    }

    endPos_ = pos;
}

//
// For the reader only:
//

size_t
RingBuffer::availableForGet(const std::string &call_id) const
{
    // Used space
    return getLength(call_id);
}

// Get will move 'toCopy' bytes from the internal FIFO to 'buffer'
size_t
RingBuffer::get(void *buffer, size_t toCopy, const std::string &call_id)
{
    if (hasNoReadPointers())
        return 0;

    if (not hasThisReadPointer(call_id))
        return 0;

    const size_t len = getLength(call_id);

    if (toCopy > len)
        toCopy = len;

    const size_t copied = toCopy;

    unsigned char *dest = (unsigned char *) buffer;
    size_t startPos = getReadPointer(call_id);

    while (toCopy > 0) {
        size_t block = toCopy;

        if (block > bufferSize_ - startPos)
            block = bufferSize_ - startPos;

        memcpy(dest, &(*buffer_.begin()) + startPos, block);
        dest += block;
        startPos = (startPos + block) % bufferSize_;
        toCopy -= block;
    }

    storeReadPointer(startPos, call_id);
    return copied;
}

size_t
RingBuffer::discard(size_t toDiscard, const std::string &call_id)
{
    size_t len = getLength(call_id);

    if (toDiscard > len)
        toDiscard = len;

    size_t startPos = (getReadPointer(call_id) + toDiscard) % bufferSize_;

    storeReadPointer(startPos, call_id);

    return toDiscard;
}
