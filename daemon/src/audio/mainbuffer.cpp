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
    // Delete any ring buffers that didn't get removed.
    // XXXX: With a good design this should never happen! :-P
    for (const auto& item : ringBufferMap_) {
        const auto& shared = item.second;
        if (shared.use_count() > 1)
            WARN("Leaking RingBuffer %s", item.first.c_str());
    }
}

void
MainBuffer::setInternalSamplingRate(unsigned sr)
{
    if (sr != internalAudioFormat_.sample_rate) {
        flushAllBuffers();
        internalAudioFormat_.sample_rate = sr;
    }
}

void
MainBuffer::setInternalAudioFormat(AudioFormat format)
{
    if (format != internalAudioFormat_) {
        flushAllBuffers();
        internalAudioFormat_ = format;
    }
}

std::shared_ptr<CallIDSet>
MainBuffer::getCallIDSet(const std::string& call_id) const
{
    const auto& iter = callIDMap_.find(call_id);
    return iter != callIDMap_.cend() ? iter->second : nullptr;
}

void
MainBuffer::removeCallIDSet(const std::string& set_id)
{
    if (not callIDMap_.erase(set_id))
        WARN("CallID set %s does not exist!", set_id.c_str());
}

void
MainBuffer::addCallIDtoSet(const std::string& set_id,
                           const std::string& call_id)
{
    auto callid_set_shared = getCallIDSet(set_id);

    // Create CallIDSet if not existing yet
    if (not callid_set_shared) {
        const auto& iter = callIDMap_.insert(std::make_pair(set_id, std::make_shared<CallIDSet>()));
        callid_set_shared = iter.first->second;
    }

    callid_set_shared->insert(call_id);
}

std::shared_ptr<RingBuffer>
MainBuffer::getRingBuffer(const std::string& id) const
{
    return ringBufferMap_.find(id)->second.lock();
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

    rbuf.reset(new RingBuffer(SIZEBUF));
    DEBUG("Ringbuffer created with id '%s'", id.c_str());
    ringBufferMap_.insert(std::make_pair(id, std::weak_ptr<RingBuffer>(rbuf)));
    return rbuf;
}

void
MainBuffer::bindOneSide(RingBuffer& rbuf, const std::string& rbuf_id,
                        const std::string& reader_id)
{
    rbuf.createReadOffset(reader_id);
    addCallIDtoSet(reader_id, rbuf_id);
}

void
MainBuffer::bindCallID(const std::string& call_id1, const std::string& call_id2)
{
    const auto& rb1 = getRingBuffer(call_id1);
    if (not rb1) {
        ERROR("No ringbuffer associated to call '%s'", call_id1.c_str());
        return;
    }

    const auto& rb2 = getRingBuffer(call_id2);
    if (not rb2) {
        ERROR("No ringbuffer associated to call '%s'", call_id2.c_str());
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    bindOneSide(*rb1, call_id1, call_id2);
    bindOneSide(*rb2, call_id2, call_id1);
}

void
MainBuffer::bindHalfDuplexOut(const std::string& process_id,
                              const std::string& call_id)
{
    // This method is used only for active calls, if this call does not exist, do nothing
    if (const auto rb = getRingBuffer(call_id)) {
        std::lock_guard<std::recursive_mutex> lk(stateLock_);

        bindOneSide(*rb, call_id, process_id);
    }
}

void
MainBuffer::unBindOneSide(const std::string& call_id1,
                          const std::string& call_id2)
{
    if (const auto callid_set_shared = getCallIDSet(call_id2))
        callid_set_shared->erase(call_id1);
    else
        WARN("CallIDSet %s does not exist!", call_id2.c_str());

    const auto ringbuffer_shared = getRingBuffer(call_id1);
    if (!ringbuffer_shared) {
        DEBUG("did not find ringbuffer %s", call_id1.c_str());
        return;
    }

    /* Don't remove read offset if still in use (i.e. in wait ) */
    if (ringbuffer_shared.use_count() >= 2) {

        /* remove them from the maps, but owners will still have
         * references to them */
        if (ringbuffer_shared->readOffsetCount() <= 1) {
            removeCallIDSet(call_id1);
        }
    } else {

        ringbuffer_shared->removeReadOffset(call_id2);

        // Remove empty RingBuffer/CallIDSet
        if (ringbuffer_shared->hasNoReadOffsets()) {
            removeCallIDSet(call_id1);
        }
    }
}

void
MainBuffer::unBindCallID(const std::string& call_id1,
                         const std::string& call_id2)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    unBindOneSide(call_id1, call_id2);
    unBindOneSide(call_id2, call_id1);
}

void
MainBuffer::unBindHalfDuplexOut(const std::string& process_id,
                                const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    unBindOneSide(call_id, process_id);

    const auto callid_set_shared = getCallIDSet(process_id);
    if (callid_set_shared and callid_set_shared->empty())
        removeCallIDSet(process_id);
}

void
MainBuffer::unBindAll(const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto callid_set_shared = getCallIDSet(call_id);
    if (!callid_set_shared or callid_set_shared->empty())
        return;

    const auto callid_set_tmp = *callid_set_shared; // temporary copy of callid_set
    for (const auto& item_set : callid_set_tmp)
        unBindCallID(call_id, item_set);
}

void
MainBuffer::putData(AudioBuffer& buffer, const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto ringbuffer_shared = getRingBuffer(call_id);
    if (ringbuffer_shared)
        ringbuffer_shared->put(buffer);
}

size_t
MainBuffer::getData(AudioBuffer& buffer, const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto rbuf_set = getCallIDSet(call_id);
    if (!rbuf_set or rbuf_set->empty())
        return 0;

    if (rbuf_set->size() == 1)
        return getDataFromRingBuffer(buffer, *rbuf_set->cbegin(), call_id);

    buffer.reset();
    buffer.setFormat(internalAudioFormat_);

    size_t size = 0;
    AudioBuffer mixBuffer(buffer);

    for (const auto& rbuf_id : *rbuf_set) {
        if (getDataFromRingBuffer(mixBuffer, rbuf_id, call_id) > 0)
            size = buffer.mix(mixBuffer);
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

    const auto callid_set_shared = getCallIDSet(call_id);
    if (!callid_set_shared or callid_set_shared->empty())
        return false;

    const auto callid_set_tmp = *callid_set_shared; // temporary copy of callid_set
    for (const auto &i : callid_set_tmp) {
        const auto ringbuffer_shared = getRingBuffer(i);
        if (!ringbuffer_shared)
            continue;
        lk.unlock();
        if (ringbuffer_shared->waitForDataAvailable(call_id, min_frames, deadline) < min_frames)
            return false;
        lk.lock();
    }
    return true;
}

size_t
MainBuffer::getAvailableData(AudioBuffer& buffer, const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto callid_set_shared = getCallIDSet(call_id);
    if (!callid_set_shared or callid_set_shared->empty())
        return 0;

    if (callid_set_shared->size() == 1) {
        const auto& iter_id = callid_set_shared->begin();
        const auto ringbuffer_shared = getRingBuffer(*iter_id);
        if (!ringbuffer_shared)
            return 0;
        return ringbuffer_shared->get(buffer, call_id);
    } else {
        size_t availableSamples = std::numeric_limits<size_t>::max();

        for (const auto &i : *callid_set_shared) {
            const auto ringbuffer_shared = getRingBuffer(i);
            if (!ringbuffer_shared) continue;
            availableSamples = std::min(availableSamples, ringbuffer_shared->availableForGet(i));
        }

        if (availableSamples == std::numeric_limits<size_t>::max())
            return 0;

        availableSamples = std::min(availableSamples, buffer.frames());
        buffer.resize(availableSamples);
        buffer.reset();
        buffer.setFormat(internalAudioFormat_);

        AudioBuffer mixBuffer(buffer);

        for (const auto &item_id : *callid_set_shared) {
            if (getDataFromRingBuffer(mixBuffer, item_id, call_id) > 0)
                buffer.mix(mixBuffer);
        }

        return availableSamples;
    }
}

size_t
MainBuffer::getDataFromRingBuffer(AudioBuffer& buffer,
                                  const std::string& rbuf_id,
                                  const std::string& reader_id)
{
    const auto ringbuffer = getRingBuffer(rbuf_id);
    return ringbuffer ? ringbuffer->get(buffer, reader_id) : 0;
}

size_t
MainBuffer::availableForGet(const std::string& call_id) const
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto callid_set_shared = getCallIDSet(call_id);
    if (!callid_set_shared or callid_set_shared->empty())
        return 0;

    if (callid_set_shared->size() == 1) {
        const auto& iter_id = callid_set_shared->begin();

        if ((call_id != DEFAULT_ID) && (*iter_id == call_id))
            DEBUG("This problem should not occur since we have %ld elements",
                  callid_set_shared->size());

        return availableForGetByID(*iter_id, call_id);
    } else {
        size_t availableSamples = std::numeric_limits<size_t>::max();

        for (const auto &i : *callid_set_shared) {
            const size_t nbSamples = availableForGetByID(i, call_id);
            if (nbSamples != 0)
                availableSamples = std::min(availableSamples, nbSamples);
        }

        return availableSamples != std::numeric_limits<size_t>::max() ? availableSamples : 0;
    }
}

size_t
MainBuffer::availableForGetByID(const std::string& call_id,
                                const std::string& reader_id) const
{
    if (call_id != DEFAULT_ID and reader_id == call_id)
        WARN("RingBuffer has a readoffset on itself");

    const auto ringbuffer_shared = getRingBuffer(call_id);
    if (ringbuffer_shared)
        return ringbuffer_shared->availableForGet(reader_id);

    WARN("RingBuffer does not exist");
    return 0;
}

size_t
MainBuffer::discard(size_t toDiscard, const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto callid_set_shared = getCallIDSet(call_id);
    if (!callid_set_shared or callid_set_shared->empty())
        return 0;

    for (const auto &item : *callid_set_shared)
        discardByID(toDiscard, item, call_id);

    return toDiscard;
}

void
MainBuffer::discardByID(size_t toDiscard, const std::string& call_id,
                        const std::string& reader_id)
{
    const auto ringbuffer_shared = getRingBuffer(call_id);
    if (ringbuffer_shared)
        ringbuffer_shared->discard(toDiscard, reader_id);
}

void
MainBuffer::flush(const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto callid_set_shared = getCallIDSet(call_id);
    if (!callid_set_shared)
        return;

    for (const auto &item : *callid_set_shared)
        flushByID(item, call_id);
}

void
MainBuffer::flushByID(const std::string& call_id, const std::string& reader_id)
{
    const auto ringbuffer_shared = getRingBuffer(call_id);
    if (ringbuffer_shared)
        ringbuffer_shared->flush(reader_id);
}

void
MainBuffer::flushAllBuffers()
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    for (const auto& item : ringBufferMap_) {
        if (const auto rb = item.second.lock())
            rb->flushAll();
    }
}
