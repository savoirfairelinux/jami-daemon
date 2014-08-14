/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author : Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "mainbuffer.h"
#include "ringbuffer.h"
#include "sfl_types.h" // for SIZEBUF
#include "logger.h"

#include <limits>
#include <utility> // for std::pair
#include <cstring>

const char * const MainBuffer::DEFAULT_ID = "audiolayer_id";

MainBuffer::MainBuffer()
{
    defaultRingBuffer = createRingBuffer(DEFAULT_ID);
}

MainBuffer::~MainBuffer()
{
    readBindingsMap_.clear();
    defaultRingBuffer.reset();

    // Verify ringbuffer not removed yet
    // XXXX: With a good design this should never happen! :-P
    for (const auto& item : ringBufferMap_) {
        const auto& weak = item.second;
        if (not weak.expired())
            WARN("Leaking RingBuffer '%s'", item.first.c_str());
    }
}

void
MainBuffer::setInternalSamplingRate(unsigned sr)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    if (sr != internalAudioFormat_.sample_rate) {
        flushAllBuffers();
        internalAudioFormat_.sample_rate = sr;
    }
}

void
MainBuffer::setInternalAudioFormat(AudioFormat format)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    if (format != internalAudioFormat_) {
        flushAllBuffers();
        internalAudioFormat_ = format;
    }
}

std::shared_ptr<RingBuffer>
MainBuffer::getRingBuffer(const std::string& id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto& it = ringBufferMap_.find(id);
    if (it != ringBufferMap_.cend()) {
        if (const auto& sptr = it->second.lock())
            return sptr;
        ringBufferMap_.erase(it);
    }

    return nullptr;
}

std::shared_ptr<RingBuffer>
MainBuffer::getRingBuffer(const std::string& id) const
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto& it = ringBufferMap_.find(id);
    if (it != ringBufferMap_.cend())
        return it->second.lock();

    return nullptr;
}

std::shared_ptr<RingBuffer>
MainBuffer::createRingBuffer(const std::string& id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    auto rbuf = getRingBuffer(id);
    if (rbuf) {
        DEBUG("Ringbuffer already exists for id '%s'", id.c_str());
        return rbuf;
    }

    rbuf.reset(new RingBuffer(id, SIZEBUF));
    DEBUG("Ringbuffer created with id '%s'", id.c_str());
    ringBufferMap_.insert(std::make_pair(id, std::weak_ptr<RingBuffer>(rbuf)));
    return rbuf;
}

const MainBuffer::ReadBindings*
MainBuffer::getReadBindings(const std::string& call_id) const
{
    const auto& iter = readBindingsMap_.find(call_id);
    return iter != readBindingsMap_.cend() ? &iter->second : nullptr;
}

MainBuffer::ReadBindings*
MainBuffer::getReadBindings(const std::string& call_id)
{
    const auto& iter = readBindingsMap_.find(call_id);
    return iter != readBindingsMap_.cend() ? &iter->second : nullptr;
}

void
MainBuffer::removeReadBindings(const std::string& call_id)
{
    if (not readBindingsMap_.erase(call_id))
        ERROR("CallID set %s does not exist!", call_id.c_str());
}

/**
 * Make given call ID a reader of given ring buffer
 */
void
MainBuffer::addReaderToRingBuffer(std::shared_ptr<RingBuffer> rbuf,
                                  const std::string& call_id)
{
    if (call_id != DEFAULT_ID and rbuf->id == call_id)
        WARN("RingBuffer has a readoffset on itself");

    rbuf->createReadOffset(call_id);
    readBindingsMap_[call_id].insert(rbuf); // bindings list created if not existing
    DEBUG("Bind rbuf '%s' to callid '%s'", rbuf->id.c_str(), call_id.c_str());
}

void
MainBuffer::removeReaderFromRingBuffer(std::shared_ptr<RingBuffer> rbuf,
                                       const std::string& call_id)
{
    if (auto bindings = getReadBindings(call_id)) {
        bindings->erase(rbuf);
        if (bindings->empty())
            removeReadBindings(call_id);
    }

    rbuf->removeReadOffset(call_id);
}

void
MainBuffer::bindCallID(const std::string& call_id1, const std::string& call_id2)
{
    const auto& rb_call1 = getRingBuffer(call_id1);
    if (not rb_call1) {
        ERROR("No ringbuffer associated to call '%s'", call_id1.c_str());
        return;
    }

    const auto& rb_call2 = getRingBuffer(call_id2);
    if (not rb_call2) {
        ERROR("No ringbuffer associated to call '%s'", call_id2.c_str());
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    addReaderToRingBuffer(rb_call1, call_id2);
    addReaderToRingBuffer(rb_call2, call_id1);
}

void
MainBuffer::bindHalfDuplexOut(const std::string& process_id,
                              const std::string& call_id)
{
    // This method is used only for active calls, if this call does not exist, do nothing
    if (const auto& rb = getRingBuffer(call_id)) {
        std::lock_guard<std::recursive_mutex> lk(stateLock_);

        addReaderToRingBuffer(rb, process_id);
    }
}

void
MainBuffer::unBindCallID(const std::string& call_id1,
                         const std::string& call_id2)
{
    const auto& rb_call1 = getRingBuffer(call_id1);
    if (not rb_call1) {
        ERROR("No ringbuffer associated to call '%s'", call_id1.c_str());
        return;
    }

    const auto& rb_call2 = getRingBuffer(call_id2);
    if (not rb_call2) {
        ERROR("No ringbuffer associated to call '%s'", call_id2.c_str());
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    removeReaderFromRingBuffer(rb_call1, call_id2);
    removeReaderFromRingBuffer(rb_call2, call_id1);
}

void
MainBuffer::unBindHalfDuplexOut(const std::string& process_id,
                                const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    if (const auto& rb = getRingBuffer(call_id)) {
        std::lock_guard<std::recursive_mutex> lk(stateLock_);

        removeReaderFromRingBuffer(rb, process_id);
    }
}

void
MainBuffer::unBindAll(const std::string& call_id)
{
    const auto& rb_call = getRingBuffer(call_id);
    if (not rb_call) {
        ERROR("No ringbuffer associated to call '%s'", call_id.c_str());
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    auto bindings = getReadBindings(call_id);
    if (not bindings)
        return;

    const auto bindings_copy = *bindings; // temporary copy
    for (const auto& rbuf : bindings_copy) {
        removeReaderFromRingBuffer(rbuf, call_id);
        removeReaderFromRingBuffer(rb_call, rbuf->id);
    }
}

size_t
MainBuffer::getData(AudioBuffer& buffer, const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto bindings = getReadBindings(call_id);
    if (not bindings)
        return 0;

    // No mixing
    if (bindings->size() == 1)
        return (*bindings->cbegin())->get(buffer, call_id);

    buffer.reset();
    buffer.setFormat(internalAudioFormat_);

    size_t size = 0;
    AudioBuffer mixBuffer(buffer);

    for (const auto& rbuf : *bindings) {
        // XXX: is it normal to only return the last positive size?
        size = rbuf->get(mixBuffer, call_id);
        if (size > 0)
            buffer.mix(mixBuffer);
    }

    return size;
}

bool
MainBuffer::waitForDataAvailable(const std::string& call_id, size_t min_frames,
                                 const std::chrono::microseconds& max_wait) const
{
    std::unique_lock<std::recursive_mutex> lk(stateLock_);

    // convert to absolute time
    const auto deadline = std::chrono::high_resolution_clock::now() + max_wait;

    auto bindings = getReadBindings(call_id);
    if (not bindings)
        return 0;

    const auto bindings_copy = *bindings; // temporary copy
    for (const auto& rbuf : bindings_copy) {
        lk.unlock();
        if (rbuf->waitForDataAvailable(call_id, min_frames, deadline) < min_frames)
            return false;
        lk.lock();
    }
    return true;
}

size_t
MainBuffer::getAvailableData(AudioBuffer& buffer, const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    auto bindings = getReadBindings(call_id);
    if (not bindings)
        return 0;

    // No mixing
    if (bindings->size() == 1) {
        return (*bindings->cbegin())->get(buffer, call_id);
    }

    size_t availableSamples = std::numeric_limits<size_t>::max();

    for (const auto& rbuf : *bindings)
        availableSamples = std::min(availableSamples, rbuf->availableForGet(call_id));

    if (availableSamples == std::numeric_limits<size_t>::max())
        return 0;

    availableSamples = std::min(availableSamples, buffer.frames());

    buffer.resize(availableSamples);
    buffer.reset();
    buffer.setFormat(internalAudioFormat_);

    AudioBuffer mixBuffer(buffer);

    for (const auto &rbuf : *bindings) {
        if (rbuf->get(mixBuffer, call_id) > 0)
            buffer.mix(mixBuffer);
    }

    return availableSamples;
}

size_t
MainBuffer::availableForGet(const std::string& call_id) const
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto bindings = getReadBindings(call_id);
    if (not bindings)
        return 0;

    // No mixing
    if (bindings->size() == 1) {
        return (*bindings->begin())->availableForGet(call_id);
    }

    size_t availableSamples = std::numeric_limits<size_t>::max();

    for (const auto& rbuf : *bindings) {
        const size_t nbSamples = rbuf->availableForGet(call_id);
        if (nbSamples > 0)
            availableSamples = std::min(availableSamples, nbSamples);
    }

    return availableSamples != std::numeric_limits<size_t>::max() ? availableSamples : 0;
}

size_t
MainBuffer::discard(size_t toDiscard, const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto bindings = getReadBindings(call_id);
    if (not bindings)
        return 0;

    for (const auto& rbuf : *bindings)
        rbuf->discard(toDiscard, call_id);

    return toDiscard;
}

void
MainBuffer::flush(const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto bindings = getReadBindings(call_id);
    if (not bindings)
        return;

    for (const auto& rbuf : *bindings)
        rbuf->flush(call_id);
}

void
MainBuffer::flushAllBuffers()
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    for (const auto& item : ringBufferMap_) {
        if (const auto rb = item.second.lock())
            rb->flushAll();
        else
            ringBufferMap_.erase(item.first);
    }
}
