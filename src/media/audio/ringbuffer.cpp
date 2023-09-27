/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
#include "client/ring_signal.h"
#include "media_buffer.h"
#include "libav_deps.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace jami {

// corresponds to 160 ms (about 5 rtp packets)
static const size_t MIN_BUFFER_SIZE = 1024;

static constexpr const int RMS_SIGNAL_INTERVAL = 5;

RingBuffer::RingBuffer(const std::string& rbuf_id, size_t /*size*/, AudioFormat format)
    : id(rbuf_id)
    , endPos_(0)
    , format_(format)
    , lock_()
    , not_empty_()
    , readoffsets_()
    , resizer_(format_, format_.sample_rate / 50, [this](std::shared_ptr<AudioFrame>&& frame) {
        putToBuffer(std::move(frame));
    })
{
    JAMI_INFO("Create new RingBuffer %s", id.c_str());
}

RingBuffer::~RingBuffer()
{
    JAMI_INFO("Destroy RingBuffer %s", id.c_str());
}

void
RingBuffer::flush(const std::string& ringbufferId)
{
    storeReadOffset(endPos_, ringbufferId);
}

void
RingBuffer::flushAll()
{
    for (auto& offset : readoffsets_)
        offset.second.offset = endPos_;
}

size_t
RingBuffer::putLength() const
{
    const size_t buffer_size = buffer_.size();
    if (buffer_size == 0)
        return 0;
    const size_t startPos = getSmallestReadOffset();
    return (endPos_ + buffer_size - startPos) % buffer_size;
}

size_t
RingBuffer::getLength(const std::string& ringbufferId) const
{
    const size_t buffer_size = buffer_.size();
    if (buffer_size == 0)
        return 0;
    return (endPos_ + buffer_size - getReadOffset(ringbufferId)) % buffer_size;
}

void
RingBuffer::debug()
{
    JAMI_DBG("Start=%zu; End=%zu; BufferSize=%zu", getSmallestReadOffset(), endPos_, buffer_.size());
}

size_t
RingBuffer::getReadOffset(const std::string& ringbufferId) const
{
    auto iter = readoffsets_.find(ringbufferId);
    return (iter != readoffsets_.end()) ? iter->second.offset : 0;
}

size_t
RingBuffer::getSmallestReadOffset() const
{
    if (hasNoReadOffsets())
        return 0;
    size_t smallest = buffer_.size();
    for (auto const& iter : readoffsets_)
        smallest = std::min(smallest, iter.second.offset);
    return smallest;
}

void
RingBuffer::storeReadOffset(size_t offset, const std::string& ringbufferId)
{
    ReadOffsetMap::iterator iter = readoffsets_.find(ringbufferId);

    if (iter != readoffsets_.end())
        iter->second.offset = offset;
    else
        JAMI_ERR("RingBuffer::storeReadOffset() failed: unknown call '%s'", ringbufferId.c_str());
}

void
RingBuffer::createReadOffset(const std::string& ringbufferId)
{
    std::lock_guard<std::mutex> l(lock_);
    if (!hasThisReadOffset(ringbufferId))
        readoffsets_.emplace(ringbufferId, ReadOffset {endPos_, {}});
}

void
RingBuffer::removeReadOffset(const std::string& ringbufferId)
{
    std::lock_guard<std::mutex> l(lock_);
    auto iter = readoffsets_.find(ringbufferId);
    if (iter != readoffsets_.end())
        readoffsets_.erase(iter);
}

bool
RingBuffer::hasThisReadOffset(const std::string& ringbufferId) const
{
    return readoffsets_.find(ringbufferId) != readoffsets_.end();
}

bool
RingBuffer::hasNoReadOffsets() const
{
    return readoffsets_.empty();
}

//
// For the writer only:
//

void
RingBuffer::put(std::shared_ptr<AudioFrame>&& data)
{
    std::lock_guard<std::mutex> l(writeLock_);
    resizer_.enqueue(resampler_.resample(std::move(data), format_));
}

// This one puts some data inside the ring buffer.
void
RingBuffer::putToBuffer(std::shared_ptr<AudioFrame>&& data)
{
    std::lock_guard<std::mutex> l(lock_);
    const size_t buffer_size = buffer_.size();
    if (buffer_size == 0)
        return;

    size_t len = buffer_size - putLength();
    if (len == 0)
        discard(1);

    size_t pos = endPos_;

    buffer_[pos] = std::move(data);
    const auto& newBuf = buffer_[pos];
    pos = (pos + 1) % buffer_size;

    endPos_ = pos;

    if (rmsSignal_) {
        ++rmsFrameCount_;
        rmsLevel_ += newBuf->calcRMS();
        if (rmsFrameCount_ == RMS_SIGNAL_INTERVAL) {
            emitSignal<libjami::AudioSignal::AudioMeter>(id, rmsLevel_ / RMS_SIGNAL_INTERVAL);
            rmsLevel_ = 0;
            rmsFrameCount_ = 0;
        }
    }

    for (auto& offset : readoffsets_) {
        if (offset.second.callback)
            offset.second.callback(newBuf);
    }

    not_empty_.notify_all();
}

//
// For the reader only:
//

size_t
RingBuffer::availableForGet(const std::string& ringbufferId) const
{
    // Used space
    return getLength(ringbufferId);
}

std::shared_ptr<AudioFrame>
RingBuffer::get(const std::string& ringbufferId)
{
    std::lock_guard<std::mutex> l(lock_);

    auto offset = readoffsets_.find(ringbufferId);
    if (offset == readoffsets_.end())
        return {};

    const size_t buffer_size = buffer_.size();
    if (buffer_size == 0)
        return {};

    size_t startPos = offset->second.offset;
    size_t len = (endPos_ + buffer_size - startPos) % buffer_size;
    if (len == 0)
        return {};

    auto ret = buffer_[startPos];
    offset->second.offset = (startPos + 1) % buffer_size;
    return ret;
}

size_t
RingBuffer::waitForDataAvailable(const std::string& ringbufferId, const time_point& deadline) const
{
    std::unique_lock<std::mutex> l(lock_);

    if (buffer_.empty())
        return 0;
    if (readoffsets_.find(ringbufferId) == readoffsets_.end())
        return 0;

    size_t getl = 0;
    auto check = [=, &getl] {
        // Re-find read_ptr: it may be destroyed during the wait
        const size_t buffer_size = buffer_.size();
        const auto read_ptr = readoffsets_.find(ringbufferId);
        if (buffer_size == 0 || read_ptr == readoffsets_.end())
            return true;
        getl = (endPos_ + buffer_size - read_ptr->second.offset) % buffer_size;
        return getl != 0;
    };

    if (deadline == time_point::max()) {
        // no timeout provided, wait as long as necessary
        not_empty_.wait(l, check);
    } else {
        not_empty_.wait_until(l, deadline, check);
    }

    return getl;
}

size_t
RingBuffer::discard(size_t toDiscard, const std::string& ringbufferId)
{
    std::lock_guard<std::mutex> l(lock_);

    const size_t buffer_size = buffer_.size();
    if (buffer_size == 0)
        return 0;

    auto offset = readoffsets_.find(ringbufferId);
    if (offset == readoffsets_.end())
        return 0;

    size_t len = (endPos_ + buffer_size - offset->second.offset) % buffer_size;
    toDiscard = std::min(toDiscard, len);

    offset->second.offset = (offset->second.offset + toDiscard) % buffer_size;
    return toDiscard;
}

size_t
RingBuffer::discard(size_t toDiscard)
{
    const size_t buffer_size = buffer_.size();
    if (buffer_size == 0)
        return 0;

    for (auto& r : readoffsets_) {
        size_t dst = (r.second.offset + buffer_size - endPos_) % buffer_size;
        if (dst < toDiscard)
            r.second.offset = (r.second.offset + toDiscard - dst) % buffer_size;
    }
    return toDiscard;
}

} // namespace jami
