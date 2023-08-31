/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "ringbufferpool.h"
#include "ringbuffer.h"
#include "ring_types.h" // for SIZEBUF
#include "logger.h"

#include <limits>
#include <utility> // for std::pair
#include <cstring>
#include <algorithm>
#pragma optimize("", off)
namespace jami {

const char* const RingBufferPool::DEFAULT_ID = "audiolayer_id";

RingBufferPool::RingBufferPool()
    : defaultRingBuffer_(createRingBuffer(DEFAULT_ID))
{}

RingBufferPool::~RingBufferPool()
{
    readBindingsMap_.clear();
    defaultRingBuffer_.reset();

    // Verify ringbuffer not removed yet
    // XXXX: With a good design this should never happen! :-P
    for (const auto& item : ringBufferMap_) {
        const auto& weak = item.second;
        if (not weak.expired())
            JAMI_WARN("Leaking RingBuffer '%s'", item.first.c_str());
    }
}

void
RingBufferPool::setInternalSamplingRate(unsigned sr)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    if (sr != internalAudioFormat_.sample_rate) {
        flushAllBuffers();
        internalAudioFormat_.sample_rate = sr;
    }
}

void
RingBufferPool::setInternalAudioFormat(AudioFormat format)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    if (format != internalAudioFormat_) {
        flushAllBuffers();
        internalAudioFormat_ = format;
        for (auto& wrb : ringBufferMap_)
            if (auto rb = wrb.second.lock())
                rb->setFormat(internalAudioFormat_);
    }
}

std::shared_ptr<RingBuffer>
RingBufferPool::getRingBuffer(const std::string& id)
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
RingBufferPool::getRingBuffer(const std::string& id) const
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto& it = ringBufferMap_.find(id);
    if (it != ringBufferMap_.cend())
        return it->second.lock();

    return nullptr;
}

std::shared_ptr<RingBuffer>
RingBufferPool::createRingBuffer(const std::string& id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    auto rbuf = getRingBuffer(id);
    if (rbuf) {
        JAMI_DBG("Ringbuffer already exists for id '%s'", id.c_str());
        return rbuf;
    }

    rbuf.reset(new RingBuffer(id, SIZEBUF, internalAudioFormat_));
    ringBufferMap_.emplace(id, std::weak_ptr<RingBuffer>(rbuf));
    return rbuf;
}

const RingBufferPool::ReadBindings*
RingBufferPool::getReadBindings(const std::string& call_id) const
{
    const auto& iter = readBindingsMap_.find(call_id);
    return iter != readBindingsMap_.cend() ? &iter->second : nullptr;
}

RingBufferPool::ReadBindings*
RingBufferPool::getReadBindings(const std::string& call_id)
{
    const auto& iter = readBindingsMap_.find(call_id);
    return iter != readBindingsMap_.cend() ? &iter->second : nullptr;
}

void
RingBufferPool::removeReadBindings(const std::string& call_id)
{
    if (not readBindingsMap_.erase(call_id))
        JAMI_ERR("CallID set %s does not exist!", call_id.c_str());
}

/**
 * Make given call ID a reader of given ring buffer
 */
void
RingBufferPool::addReaderToRingBuffer(const std::shared_ptr<RingBuffer>& rbuf,
                                      const std::string& call_id)
{
    if (call_id != DEFAULT_ID and rbuf->getId() == call_id)
        JAMI_WARN("RingBuffer has a readoffset on itself");

    rbuf->createReadOffset(call_id);
    readBindingsMap_[call_id].insert(rbuf); // bindings list created if not existing
    JAMI_DBG("Bind rbuf '%s' to callid '%s'", rbuf->getId().c_str(), call_id.c_str());
}

void
RingBufferPool::removeReaderFromRingBuffer(const std::shared_ptr<RingBuffer>& rbuf,
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
RingBufferPool::bindCallID(const std::string& call_id1, const std::string& call_id2)
{
    JAMI_INFO("Bind call %s to call %s", call_id1.c_str(), call_id2.c_str());

    const auto& rb_call1 = getRingBuffer(call_id1);
    if (not rb_call1) {
        JAMI_ERR("No ringbuffer associated with call '%s'", call_id1.c_str());
        return;
    }

    const auto& rb_call2 = getRingBuffer(call_id2);
    if (not rb_call2) {
        JAMI_ERR("No ringbuffer associated to call '%s'", call_id2.c_str());
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    addReaderToRingBuffer(rb_call1, call_id2);
    addReaderToRingBuffer(rb_call2, call_id1);
}

void
RingBufferPool::bindHalfDuplexOut(const std::string& process_id, const std::string& call_id)
{
    /* This method is used only for active calls, if this call does not exist,
     * do nothing */
    if (const auto& rb = getRingBuffer(call_id)) {
        std::lock_guard<std::recursive_mutex> lk(stateLock_);

        addReaderToRingBuffer(rb, process_id);
    }
}

void
RingBufferPool::unBindCallID(const std::string& call_id1, const std::string& call_id2)
{
    JAMI_INFO("Unbind calls %s and %s", call_id1.c_str(), call_id2.c_str());

    const auto& rb_call1 = getRingBuffer(call_id1);
    if (not rb_call1) {
        JAMI_ERR("No ringbuffer associated to call '%s'", call_id1.c_str());
        return;
    }

    const auto& rb_call2 = getRingBuffer(call_id2);
    if (not rb_call2) {
        JAMI_ERR("No ringbuffer associated to call '%s'", call_id2.c_str());
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    removeReaderFromRingBuffer(rb_call1, call_id2);
    removeReaderFromRingBuffer(rb_call2, call_id1);
}

void
RingBufferPool::unBindHalfDuplexOut(const std::string& process_id, const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    if (const auto& rb = getRingBuffer(call_id))
        removeReaderFromRingBuffer(rb, process_id);
}

void
RingBufferPool::unBindAllHalfDuplexOut(const std::string& call_id)
{
    const auto& rb_call = getRingBuffer(call_id);
    if (not rb_call) {
        JAMI_ERR("No ringbuffer associated to call '%s'", call_id.c_str());
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    auto bindings = getReadBindings(call_id);
    if (not bindings)
        return;

    const auto bindings_copy = *bindings; // temporary copy
    for (const auto& rbuf : bindings_copy) {
        removeReaderFromRingBuffer(rb_call, rbuf->getId());
    }
}

void
RingBufferPool::unBindAll(const std::string& call_id)
{
    JAMI_INFO("Unbind call %s from all bound calls", call_id.c_str());

    const auto& rb_call = getRingBuffer(call_id);
    if (not rb_call) {
        JAMI_ERR("No ringbuffer associated to call '%s'", call_id.c_str());
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    auto bindings = getReadBindings(call_id);
    if (not bindings)
        return;

    const auto bindings_copy = *bindings; // temporary copy
    for (const auto& rbuf : bindings_copy) {
        removeReaderFromRingBuffer(rbuf, call_id);
        removeReaderFromRingBuffer(rb_call, rbuf->getId());
    }
}

std::shared_ptr<AudioFrame>
RingBufferPool::getData(const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto bindings = getReadBindings(call_id);
    if (not bindings)
        return {};

    // No mixing
    if (bindings->size() == 1)
        return (*bindings->cbegin())->get(call_id);

    auto mixBuffer = std::make_shared<AudioFrame>(internalAudioFormat_);
    auto mixed = false;
    for (const auto& rbuf : *bindings) {
        if (auto b = rbuf->get(call_id)) {
            mixed = true;
            mixBuffer->mix(*b);

            // voice is true if any of mixed frames has voice
            mixBuffer->has_voice |= b->has_voice;
        }
    }

    return mixed ? mixBuffer : nullptr;
}

bool
RingBufferPool::waitForDataAvailable(const std::string& call_id,
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
        if (rbuf->waitForDataAvailable(call_id, deadline) == 0)
            return false;
        lk.lock();
    }
    return true;
}

std::shared_ptr<AudioFrame>
RingBufferPool::getAvailableData(const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    auto bindings = getReadBindings(call_id);
    if (not bindings)
        return 0;

    // No mixing
    if (bindings->size() == 1) {
        return (*bindings->cbegin())->get(call_id);
    }

    size_t availableFrames = 0;

    for (const auto& rbuf : *bindings)
        availableFrames = std::min(availableFrames, rbuf->availableForGet(call_id));

    if (availableFrames == 0)
        return {};

    auto buf = std::make_shared<AudioFrame>(internalAudioFormat_);
    for (const auto& rbuf : *bindings) {
        if (auto b = rbuf->get(call_id)) {
            buf->mix(*b);

            // voice is true if any of mixed frames has voice
            buf->has_voice |= b->has_voice;
        }
    }

    return buf;
}

size_t
RingBufferPool::availableForGet(const std::string& call_id) const
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
        if (nbSamples != 0)
            availableSamples = std::min(availableSamples, nbSamples);
    }

    return availableSamples != std::numeric_limits<size_t>::max() ? availableSamples : 0;
}

size_t
RingBufferPool::discard(size_t toDiscard, const std::string& call_id)
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
RingBufferPool::flush(const std::string& call_id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto bindings = getReadBindings(call_id);
    if (not bindings)
        return;

    for (const auto& rbuf : *bindings)
        rbuf->flush(call_id);
}

void
RingBufferPool::flushAllBuffers()
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    for (auto item = ringBufferMap_.begin(); item != ringBufferMap_.end();) {
        if (const auto rb = item->second.lock()) {
            rb->flushAll();
            ++item;
        } else {
            // Use this version of erase to avoid using invalidated iterator
            item = ringBufferMap_.erase(item);
        }
    }
}

bool
RingBufferPool::isAudioMeterActive(const std::string& id)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);
    if (!id.empty()) {
        if (auto rb = getRingBuffer(id)) {
            return rb->isAudioMeterActive();
        }
    } else {
        for (auto item = ringBufferMap_.begin(); item != ringBufferMap_.end(); ++item) {
            if (const auto rb = item->second.lock()) {
                if (rb->isAudioMeterActive()) {
                    return true;
                }
            }
        }
    }
    return false;
}

void
RingBufferPool::setAudioMeterState(const std::string& id, bool state)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);
    if (!id.empty()) {
        if (auto rb = getRingBuffer(id)) {
            rb->setAudioMeterState(state);
        }
    } else {
        for (auto item = ringBufferMap_.begin(); item != ringBufferMap_.end(); ++item) {
            if (const auto rb = item->second.lock()) {
                rb->setAudioMeterState(state);
            }
        }
    }
}

} // namespace jami
