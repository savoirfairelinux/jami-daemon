/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

MainBuffer::MainBuffer() : ringBufferMap_(), callIDMap_(), stateLock_(), internalAudioFormat_(AudioFormat::MONO)
{}

MainBuffer::~MainBuffer()
{
    // delete any ring buffers that didn't get removed
    for (auto &item : ringBufferMap_)
        delete item.second;
}

void MainBuffer::setInternalSamplingRate(unsigned sr)
{
    if (sr != internalAudioFormat_.sample_rate) {
        flushAllBuffers();
        internalAudioFormat_.sample_rate = sr;
    }
}

void MainBuffer::setInternalAudioFormat(AudioFormat format)
{
    if (format != internalAudioFormat_) {
        flushAllBuffers();
        internalAudioFormat_ = format;
    }
}

CallIDSet* MainBuffer::getCallIDSet(const std::string &call_id)
{
    CallIDMap::iterator iter = callIDMap_.find(call_id);
    return (iter != callIDMap_.end()) ? iter->second : nullptr;
}

const CallIDSet* MainBuffer::getCallIDSet(const std::string &call_id) const
{
    CallIDMap::const_iterator iter = callIDMap_.find(call_id);
    return (iter != callIDMap_.end()) ? iter->second : nullptr;
}

void MainBuffer::createCallIDSet(const std::string &set_id)
{
    if (getCallIDSet(set_id) == nullptr)
        callIDMap_[set_id] = new CallIDSet;
    else
        DEBUG("CallID set %s already exists, ignoring", set_id.c_str());
}

void MainBuffer::removeCallIDSet(const std::string &set_id)
{
    CallIDSet* callid_set = getCallIDSet(set_id);

    if (callid_set) {
        callIDMap_.erase(set_id);
        delete callid_set;
    } else
        ERROR("CallID set %s does not exist!", set_id.c_str());
}

void MainBuffer::addCallIDtoSet(const std::string &set_id, const std::string &call_id)
{
    CallIDSet* callid_set = getCallIDSet(set_id);

    if (callid_set)
        callid_set->insert(call_id);
    else
        ERROR("CallIDSet %s does not exist!", set_id.c_str());
}

void MainBuffer::removeCallIDfromSet(const std::string &set_id, const std::string &call_id)
{
    CallIDSet* callid_set = getCallIDSet(set_id);

    if (callid_set)
        callid_set->erase(call_id);
    else
        ERROR("CallIDSet %s does not exist!", set_id.c_str());
}

RingBuffer* MainBuffer::getRingBuffer(const std::string & call_id)
{
    RingBufferMap::iterator iter = ringBufferMap_.find(call_id);
    return (iter != ringBufferMap_.end()) ? iter->second : nullptr;
}

const RingBuffer* MainBuffer::getRingBuffer(const std::string & call_id) const
{
    RingBufferMap::const_iterator iter = ringBufferMap_.find(call_id);
    return (iter != ringBufferMap_.end()) ? iter->second : nullptr;
}

void MainBuffer::createRingBuffer(const std::string &call_id)
{
    if (!getRingBuffer(call_id))
        ringBufferMap_[call_id] = new RingBuffer(SIZEBUF, call_id);
    else
        DEBUG("Ringbuffer already exists for call_id %s", call_id.c_str());
}

void MainBuffer::removeRingBuffer(const std::string &call_id)
{
    RingBuffer* ring_buffer = getRingBuffer(call_id);

    if (ring_buffer) {
        ringBufferMap_.erase(call_id);
        delete ring_buffer;
    } else
        DEBUG("Ringbuffer %s does not exist!", call_id.c_str());
}

void MainBuffer::bindCallID(const std::string & call_id1, const std::string & call_id2)
{
    auto lock(stateLock_.write());

    createRingBuffer(call_id1);
    createCallIDSet(call_id1);
    createRingBuffer(call_id2);
    createCallIDSet(call_id2);

    getRingBuffer(call_id1)->createReadPointer(call_id2);
    getRingBuffer(call_id2)->createReadPointer(call_id1);
    addCallIDtoSet(call_id1, call_id2);
    addCallIDtoSet(call_id2, call_id1);
}

void MainBuffer::bindHalfDuplexOut(const std::string & process_id, const std::string & call_id)
{
    auto lock(stateLock_.write());

    // This method is used only for active calls, if this call does not exist, do nothing
    if (!getRingBuffer(call_id))
        return;

    createCallIDSet(process_id);
    getRingBuffer(call_id)->createReadPointer(process_id);
    addCallIDtoSet(process_id, call_id);
}

void MainBuffer::unBindCallID(const std::string & call_id1, const std::string & call_id2)
{
    auto lock(stateLock_.write());

    removeCallIDfromSet(call_id1, call_id2);
    removeCallIDfromSet(call_id2, call_id1);

    RingBuffer* ringbuffer = getRingBuffer(call_id2);

    if (ringbuffer) {
        ringbuffer->removeReadPointer(call_id1);

        if (ringbuffer->hasNoReadPointers()) {
            removeCallIDSet(call_id2);
            removeRingBuffer(call_id2);
        }
    }

    ringbuffer = getRingBuffer(call_id1);

    if (ringbuffer) {
        ringbuffer->removeReadPointer(call_id2);

        if (ringbuffer->hasNoReadPointers()) {
            removeCallIDSet(call_id1);
            removeRingBuffer(call_id1);
        }
    }
}

void MainBuffer::unBindHalfDuplexOut(const std::string & process_id, const std::string & call_id)
{
    auto lock(stateLock_.write());

    removeCallIDfromSet(process_id, call_id);

    RingBuffer* ringbuffer = getRingBuffer(call_id);

    if (ringbuffer) {
        ringbuffer->removeReadPointer(process_id);

        if (ringbuffer->hasNoReadPointers()) {
            removeCallIDSet(call_id);
            removeRingBuffer(call_id);
        }
    } else {
        DEBUG("did not found ringbuffer %s", process_id.c_str());
        removeCallIDSet(process_id);
    }

    CallIDSet* callid_set = getCallIDSet(process_id);

    if (callid_set and callid_set->empty())
        removeCallIDSet(process_id);
}

void MainBuffer::unBindAll(const std::string & call_id)
{
    CallIDSet* callid_set = getCallIDSet(call_id);

    if (callid_set == nullptr or callid_set->empty())
        return;

    CallIDSet temp_set(*callid_set);

    for (const auto &item_set : temp_set)
        unBindCallID(call_id, item_set);
}

void MainBuffer::putData(AudioBuffer& buffer, const std::string &call_id)
{
    auto lock(stateLock_.read());

    RingBuffer* ring_buffer = getRingBuffer(call_id);

    if (ring_buffer)
        ring_buffer->put(buffer);
}

size_t MainBuffer::getData(AudioBuffer& buffer, const std::string &call_id)
{
    auto lock(stateLock_.read());

    CallIDSet* callid_set = getCallIDSet(call_id);

    if (callid_set == nullptr or callid_set->empty())
        return 0;

    if (callid_set->size() == 1) {

        CallIDSet::iterator iter_id = callid_set->begin();

        if (iter_id != callid_set->end())
            return getDataByID(buffer, *iter_id, call_id);
        else
            return 0;
    } else {
        buffer.reset();
        buffer.setFormat(internalAudioFormat_);

        size_t size = 0;
        AudioBuffer mixBuffer(buffer);

        for (const auto &item_id : *callid_set) {
            size = getDataByID(mixBuffer, item_id, call_id);

            if (size > 0) {
                buffer.mix(mixBuffer);
            }
        }

        return size;
    }
}

bool MainBuffer::waitForDataAvailable(const std::string &call_id, size_t min_frames, const std::chrono::microseconds& max_wait) const
{
    auto deadline = (max_wait == std::chrono::microseconds()) ?
        std::chrono::high_resolution_clock::time_point() :
        std::chrono::high_resolution_clock::now() + max_wait;
    auto lock(stateLock_.read());
    const CallIDSet* callid_set = getCallIDSet(call_id);
    if (!callid_set or callid_set->empty()) return false;
    for (const auto &i : *callid_set) {
        RingBuffer const * const ringbuffer = getRingBuffer(i);
        if (!ringbuffer) continue;
        if (ringbuffer->waitForDataAvailable(call_id, min_frames, deadline) < min_frames) return false;
    }
    return true;
}


size_t MainBuffer::getAvailableData(AudioBuffer& buffer, const std::string &call_id)
{
    auto lock(stateLock_.read());

    CallIDSet* callid_set = getCallIDSet(call_id);
    if (callid_set == nullptr or callid_set->empty())
        return 0;

    if (callid_set->size() == 1) {
        CallIDSet::iterator iter_id = callid_set->begin();
        RingBuffer *const ringbuffer = getRingBuffer(*iter_id);
        if (!ringbuffer) return 0;
        return ringbuffer->get(buffer, call_id);
    } else {
        size_t availableSamples = std::numeric_limits<size_t>::max();
        for (const auto &i : *callid_set) {
            const RingBuffer* ringbuffer = getRingBuffer(i);
            if (!ringbuffer) continue;
            availableSamples = std::min(availableSamples, ringbuffer->availableForGet(i));
        }
        if (availableSamples == std::numeric_limits<size_t>::max())
            return 0;
        availableSamples = std::min(availableSamples, buffer.frames());
        buffer.resize(availableSamples);
        buffer.reset();
        buffer.setFormat(internalAudioFormat_);

        size_t size = 0;
        AudioBuffer mixBuffer(buffer);

        for (const auto &item_id : *callid_set) {
            size = getDataByID(mixBuffer, item_id, call_id);
            if (size > 0) {
                buffer.mix(mixBuffer);
            }
        }

        return availableSamples;
    }
}

size_t MainBuffer::getDataByID(AudioBuffer& buffer, const std::string & call_id, const std::string & reader_id)
{
    RingBuffer* ring_buffer = getRingBuffer(call_id);
    return ring_buffer ? ring_buffer->get(buffer, reader_id) : 0;
}

size_t MainBuffer::availableForGet(const std::string &call_id) const
{
    auto lock(stateLock_.read());

    const CallIDSet* callid_set = getCallIDSet(call_id);

    if (callid_set == nullptr or callid_set->empty())
        return 0;

    if (callid_set->size() == 1) {
        CallIDSet::iterator iter_id = callid_set->begin();

        if ((call_id != DEFAULT_ID) && (*iter_id == call_id))
            DEBUG("This problem should not occur since we have %ld elements", callid_set->size());

        return availableForGetByID(*iter_id, call_id);

    } else {

        size_t availableSamples = std::numeric_limits<size_t>::max();

        for (const auto &i : *callid_set) {
            const size_t nbSamples = availableForGetByID(i, call_id);

            if (nbSamples != 0)
                availableSamples = std::min(availableSamples, nbSamples);
        }

        return availableSamples != std::numeric_limits<size_t>::max() ? availableSamples : 0;
    }
}

size_t MainBuffer::availableForGetByID(const std::string &call_id,
                                       const std::string &reader_id) const
{
    if (call_id != DEFAULT_ID and reader_id == call_id)
        ERROR("RingBuffer has a readpointer on itself");

    const RingBuffer* ringbuffer = getRingBuffer(call_id);

    if (ringbuffer == nullptr) {
        ERROR("RingBuffer does not exist");
        return 0;
    } else
        return ringbuffer->availableForGet(reader_id);

}

size_t MainBuffer::discard(size_t toDiscard, const std::string &call_id)
{
    auto lock(stateLock_.read());

    CallIDSet* callid_set = getCallIDSet(call_id);

    if (!callid_set or callid_set->empty())
        return 0;

    for (auto &item : *callid_set)
        discardByID(toDiscard, item, call_id);

    return toDiscard;
}

void MainBuffer::discardByID(size_t toDiscard, const std::string & call_id, const std::string & reader_id)
{
    RingBuffer* ringbuffer = getRingBuffer(call_id);

    if (ringbuffer)
        ringbuffer->discard(toDiscard, reader_id);
}

void MainBuffer::flush(const std::string & call_id)
{
    auto lock(stateLock_.read());

    CallIDSet* callid_set = getCallIDSet(call_id);
    if (callid_set == nullptr)
        return;

    for (auto &item : *callid_set)
        flushByID(item, call_id);
}

void MainBuffer::flushByID(const std::string & call_id, const std::string & reader_id)
{
    RingBuffer* ringbuffer = getRingBuffer(call_id);
    if (ringbuffer)
        ringbuffer->flush(reader_id);
}

void MainBuffer::flushAllBuffers()
{
    auto lock(stateLock_.read());

    for (auto &item : ringBufferMap_)
        item.second->flushAll();
}
