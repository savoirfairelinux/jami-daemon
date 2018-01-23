/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Adrien Beraud <adrien.beraud@gmail.com>
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
 */

#include "ringbuffer.h"
#include "logger.h"

#include <chrono>
#include <utility> // for std::pair
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace ring {

// corresponds to 160 ms (about 5 rtp packets)
static const size_t MIN_BUFFER_SIZE = 1024;

// Create  a ring buffer with 'size' bytes
RingBuffer::RingBuffer(const std::string& rbuf_id, size_t size,
                       AudioFormat format /* = MONO */)
    : id(rbuf_id)
    , endPos_(0)
    , buffer_(std::max(size, MIN_BUFFER_SIZE), format)
    , lock_()
    , not_empty_()
    , readoffsets_()
{}

void
RingBuffer::flush(const std::string &call_id)
{
    storeReadOffset(endPos_, call_id);
}


void
RingBuffer::flushAll()
{
    ReadOffset::iterator iter;

    for (iter = readoffsets_.begin(); iter != readoffsets_.end(); ++iter)
        iter->second = endPos_;
}

size_t
RingBuffer::putLength() const
{
    const size_t buffer_size = buffer_.frames();
    if (buffer_size == 0)
        return 0;
    const size_t startPos = (not readoffsets_.empty()) ? getSmallestReadOffset() : 0;
    return (endPos_ + buffer_size - startPos) % buffer_size;
}

size_t RingBuffer::getLength(const std::string &call_id) const
{
    const size_t buffer_size = buffer_.frames();
    if (buffer_size == 0)
        return 0;
    return (endPos_ + buffer_size - getReadOffset(call_id)) % buffer_size;
}

void
RingBuffer::debug()
{
    RING_DBG("Start=%zu; End=%zu; BufferSize=%zu", getSmallestReadOffset(), endPos_, buffer_.frames());
}

size_t RingBuffer::getReadOffset(const std::string &call_id) const
{
    if (hasNoReadOffsets())
        return 0;
    ReadOffset::const_iterator iter = readoffsets_.find(call_id);
    return (iter != readoffsets_.end()) ? iter->second : 0;
}

size_t
RingBuffer::getSmallestReadOffset() const
{
    if (hasNoReadOffsets())
        return 0;
    size_t smallest = buffer_.frames();
    for(auto const& iter : readoffsets_)
        smallest = std::min(smallest, iter.second);
    return smallest;
}

void
RingBuffer::storeReadOffset(size_t offset, const std::string &call_id)
{
    ReadOffset::iterator iter = readoffsets_.find(call_id);

    if (iter != readoffsets_.end())
        iter->second = offset;
    else
        RING_ERR("RingBuffer::storeReadOffset() failed: unknown call '%s'", call_id.c_str());
}


void
RingBuffer::createReadOffset(const std::string &call_id)
{
    std::lock_guard<std::mutex> l(lock_);
    if (!hasThisReadOffset(call_id))
        readoffsets_.insert(std::pair<std::string, int> (call_id, endPos_));
}


void
RingBuffer::removeReadOffset(const std::string &call_id)
{
    std::lock_guard<std::mutex> l(lock_);
    ReadOffset::iterator iter = readoffsets_.find(call_id);

    if (iter != readoffsets_.end())
        readoffsets_.erase(iter);
}


bool
RingBuffer::hasThisReadOffset(const std::string &call_id) const
{
    return readoffsets_.find(call_id) != readoffsets_.end();
}


bool RingBuffer::hasNoReadOffsets() const
{
    return readoffsets_.empty();
}

//
// For the writer only:
//

// This one puts some data inside the ring buffer.
void RingBuffer::put(AudioBuffer& buf)
{
    std::lock_guard<std::mutex> l(lock_);
    const size_t sample_num = buf.frames();
    const size_t buffer_size = buffer_.frames();
    if (buffer_size == 0)
        return;

    size_t len = putLength();
    if (buffer_size - len < sample_num)
        discard(sample_num);
    size_t toCopy = sample_num;

    // Add more channels if the input buffer holds more channels than the ring.
    if (buffer_.channels() < buf.channels())
        buffer_.setChannelNum(buf.channels());

    size_t in_pos = 0;
    size_t pos = endPos_;

    while (toCopy) {
        size_t block = toCopy;

        if (block > buffer_size - pos) // Wrap block around ring ?
            block = buffer_size - pos; // Fill in to the end of the buffer

        buffer_.copy(buf, block, in_pos, pos);
        in_pos += block;
        pos = (pos + block) % buffer_size;
        toCopy -= block;
    }

    endPos_ = pos;
    not_empty_.notify_all();
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

size_t RingBuffer::get(AudioBuffer& buf, const std::string &call_id)
{
    std::lock_guard<std::mutex> l(lock_);

    if (hasNoReadOffsets())
        return 0;

    if (not hasThisReadOffset(call_id))
        return 0;

    const size_t buffer_size = buffer_.frames();
    if (buffer_size == 0)
        return 0;

    size_t len = getLength(call_id);
    const size_t sample_num = buf.frames();
    size_t toCopy = std::min(sample_num, len);
    if (toCopy and toCopy != sample_num) {
        RING_DBG("Partial get: %zu/%zu", toCopy, sample_num);
    }

    const size_t copied = toCopy;

    size_t dest = 0;
    size_t startPos = getReadOffset(call_id);

    while (toCopy > 0) {
        size_t block = toCopy;

        if (block > buffer_size - startPos)
            block = buffer_size - startPos;

        buf.copy(buffer_, block, startPos, dest);

        dest += block;
        startPos = (startPos + block) % buffer_size;
        toCopy -= block;
    }

    storeReadOffset(startPos, call_id);
    return copied;
}


size_t RingBuffer::waitForDataAvailable(const std::string &call_id, const size_t min_data_length, const std::chrono::high_resolution_clock::time_point& deadline) const
{
    std::unique_lock<std::mutex> l(lock_);
    const size_t buffer_size = buffer_.frames();
    if (buffer_size < min_data_length) return 0;
    ReadOffset::const_iterator read_ptr = readoffsets_.find(call_id);
    if (read_ptr == readoffsets_.end()) return 0;
    size_t getl = 0;
    if (deadline == std::chrono::high_resolution_clock::time_point()) {
        not_empty_.wait(l, [=, &getl] {
                // Re-find read_ptr: it may be destroyed during the wait
                const auto read_ptr = readoffsets_.find(call_id);
                if (read_ptr == readoffsets_.end())
                    return true;
                getl = (endPos_ + buffer_size - read_ptr->second) % buffer_size;
                return getl >= min_data_length;
        });
    } else {
        not_empty_.wait_until(l, deadline, [=, &getl]{
                // Re-find read_ptr: it may be destroyed during the wait
                const auto read_ptr = readoffsets_.find(call_id);
                if (read_ptr == readoffsets_.end())
                    return true;
                getl = (endPos_ + buffer_size - read_ptr->second) % buffer_size;
                return getl >= min_data_length;
        });
    }
    return getl;
}

size_t
RingBuffer::discard(size_t toDiscard, const std::string &call_id)
{
    std::lock_guard<std::mutex> l(lock_);

    const size_t buffer_size = buffer_.frames();
    if (buffer_size == 0)
        return 0;

    size_t len = getLength(call_id);
    if (toDiscard > len)
        toDiscard = len;

    size_t startPos = (getReadOffset(call_id) + toDiscard) % buffer_size;
    storeReadOffset(startPos, call_id);
    return toDiscard;
}

size_t
RingBuffer::discard(size_t toDiscard)
{
    const size_t buffer_size = buffer_.frames();
    if (buffer_size == 0)
        return 0;

    for (auto & r : readoffsets_) {
        size_t dst = (r.second + buffer_size - endPos_) % buffer_size;
        if (dst < toDiscard)
            r.second = (r.second + toDiscard - dst) % buffer_size;
    }
    return toDiscard;
}

} // namespace ring
