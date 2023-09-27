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
            JAMI_WARNING("Leaking RingBuffer '{}'", item.first);
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
        JAMI_DEBUG("Ringbuffer already exists for id '{}'", id);
        return rbuf;
    }

    rbuf.reset(new RingBuffer(id, SIZEBUF, internalAudioFormat_));
    ringBufferMap_.emplace(id, std::weak_ptr<RingBuffer>(rbuf));
    return rbuf;
}

const RingBufferPool::ReadBindings*
RingBufferPool::getReadBindings(const std::string& ringbufferId) const
{
    const auto& iter = readBindingsMap_.find(ringbufferId);
    return iter != readBindingsMap_.cend() ? &iter->second : nullptr;
}

RingBufferPool::ReadBindings*
RingBufferPool::getReadBindings(const std::string& ringbufferId)
{
    const auto& iter = readBindingsMap_.find(ringbufferId);
    return iter != readBindingsMap_.cend() ? &iter->second : nullptr;
}

void
RingBufferPool::removeReadBindings(const std::string& ringbufferId)
{
    if (not readBindingsMap_.erase(ringbufferId))
        JAMI_ERROR("Ringbuffer {} does not exist!", ringbufferId);
}

/**
 * Make given ringbuffer a reader of given ring buffer
 */
void
RingBufferPool::addReaderToRingBuffer(const std::shared_ptr<RingBuffer>& rbuf,
                                      const std::string& ringbufferId)
{
    if (ringbufferId != DEFAULT_ID and rbuf->getId() == ringbufferId)
        JAMI_WARNING("RingBuffer has a readoffset on itself");

    rbuf->createReadOffset(ringbufferId);
    readBindingsMap_[ringbufferId].insert(rbuf); // bindings list created if not existing
    JAMI_DEBUG("Bind rbuf '{}' to ringbuffer '{}'", rbuf->getId(), ringbufferId);
}

void
RingBufferPool::removeReaderFromRingBuffer(const std::shared_ptr<RingBuffer>& rbuf,
                                           const std::string& ringbufferId)
{
    if (auto bindings = getReadBindings(ringbufferId)) {
        bindings->erase(rbuf);
        if (bindings->empty())
            removeReadBindings(ringbufferId);
    }

    rbuf->removeReadOffset(ringbufferId);
}

void
RingBufferPool::bindRingbuffers(const std::string& ringbufferId1, const std::string& ringbufferId2)
{
    JAMI_LOG("Bind ringbuffer {} to ringbuffer {}", ringbufferId1, ringbufferId2);

    const auto& rb1 = getRingBuffer(ringbufferId1);
    if (not rb1) {
        JAMI_ERROR("No ringbuffer associated with id '{}'", ringbufferId1);
        return;
    }

    const auto& rb2 = getRingBuffer(ringbufferId2);
    if (not rb2) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", ringbufferId2);
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    addReaderToRingBuffer(rb1, ringbufferId2);
    addReaderToRingBuffer(rb2, ringbufferId1);
}

void
RingBufferPool::bindHalfDuplexOut(const std::string& processId, const std::string& ringbufferId)
{
    /* This method is used only for active ringbuffers, if this ringbuffer does not exist,
     * do nothing */
    if (const auto& rb = getRingBuffer(ringbufferId)) {
        std::lock_guard<std::recursive_mutex> lk(stateLock_);

        addReaderToRingBuffer(rb, processId);
    }
}

void
RingBufferPool::unbindRingbuffers(const std::string& ringbufferId1, const std::string& ringbufferId2)
{
    JAMI_LOG("Unbind ringbuffers {} and {}", ringbufferId1, ringbufferId2);

    const auto& rb1 = getRingBuffer(ringbufferId1);
    if (not rb1) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", ringbufferId1);
        return;
    }

    const auto& rb2 = getRingBuffer(ringbufferId2);
    if (not rb2) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", ringbufferId2);
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    removeReaderFromRingBuffer(rb1, ringbufferId2);
    removeReaderFromRingBuffer(rb2, ringbufferId1);
}

void
RingBufferPool::unBindHalfDuplexOut(const std::string& process_id, const std::string& ringbufferId)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    if (const auto& rb = getRingBuffer(ringbufferId))
        removeReaderFromRingBuffer(rb, process_id);
}

void
RingBufferPool::unBindAllHalfDuplexOut(const std::string& ringbufferId)
{
    const auto& rb = getRingBuffer(ringbufferId);
    if (not rb) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", ringbufferId);
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    auto bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return;

    const auto bindings_copy = *bindings; // temporary copy
    for (const auto& rbuf : bindings_copy) {
        removeReaderFromRingBuffer(rb, rbuf->getId());
    }
}

void
RingBufferPool::unBindAll(const std::string& ringbufferId)
{
    JAMI_LOG("Unbind ringbuffer {} from all bound ringbuffers", ringbufferId);

    const auto& rb = getRingBuffer(ringbufferId);
    if (not rb) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", ringbufferId);
        return;
    }

    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    auto bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return;

    const auto bindings_copy = *bindings; // temporary copy
    for (const auto& rbuf : bindings_copy) {
        removeReaderFromRingBuffer(rbuf, ringbufferId);
        removeReaderFromRingBuffer(rb, rbuf->getId());
    }
}

std::shared_ptr<AudioFrame>
RingBufferPool::getData(const std::string& ringbufferId)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return {};

    // No mixing
    if (bindings->size() == 1)
        return (*bindings->cbegin())->get(ringbufferId);

    auto mixBuffer = std::make_shared<AudioFrame>(internalAudioFormat_);
    auto mixed = false;
    for (const auto& rbuf : *bindings) {
        if (auto b = rbuf->get(ringbufferId)) {
            mixed = true;
            mixBuffer->mix(*b);

            // voice is true if any of mixed frames has voice
            mixBuffer->has_voice |= b->has_voice;
        }
    }

    return mixed ? mixBuffer : nullptr;
}

bool
RingBufferPool::waitForDataAvailable(const std::string& ringbufferId,
                                     const std::chrono::microseconds& max_wait) const
{
    std::unique_lock<std::recursive_mutex> lk(stateLock_);

    // convert to absolute time
    const auto deadline = std::chrono::high_resolution_clock::now() + max_wait;

    auto bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return 0;

    const auto bindings_copy = *bindings; // temporary copy
    for (const auto& rbuf : bindings_copy) {
        lk.unlock();
        if (rbuf->waitForDataAvailable(ringbufferId, deadline) == 0)
            return false;
        lk.lock();
    }
    return true;
}

std::shared_ptr<AudioFrame>
RingBufferPool::getAvailableData(const std::string& ringbufferId)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    auto bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return 0;

    // No mixing
    if (bindings->size() == 1) {
        return (*bindings->cbegin())->get(ringbufferId);
    }

    size_t availableFrames = 0;

    for (const auto& rbuf : *bindings)
        availableFrames = std::min(availableFrames, rbuf->availableForGet(ringbufferId));

    if (availableFrames == 0)
        return {};

    auto buf = std::make_shared<AudioFrame>(internalAudioFormat_);
    for (const auto& rbuf : *bindings) {
        if (auto b = rbuf->get(ringbufferId)) {
            buf->mix(*b);

            // voice is true if any of mixed frames has voice
            buf->has_voice |= b->has_voice;
        }
    }

    return buf;
}

size_t
RingBufferPool::availableForGet(const std::string& ringbufferId) const
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return 0;

    // No mixing
    if (bindings->size() == 1) {
        return (*bindings->begin())->availableForGet(ringbufferId);
    }

    size_t availableSamples = std::numeric_limits<size_t>::max();

    for (const auto& rbuf : *bindings) {
        const size_t nbSamples = rbuf->availableForGet(ringbufferId);
        if (nbSamples != 0)
            availableSamples = std::min(availableSamples, nbSamples);
    }

    return availableSamples != std::numeric_limits<size_t>::max() ? availableSamples : 0;
}

size_t
RingBufferPool::discard(size_t toDiscard, const std::string& ringbufferId)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return 0;

    for (const auto& rbuf : *bindings)
        rbuf->discard(toDiscard, ringbufferId);

    return toDiscard;
}

void
RingBufferPool::flush(const std::string& ringbufferId)
{
    std::lock_guard<std::recursive_mutex> lk(stateLock_);

    const auto bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return;

    for (const auto& rbuf : *bindings)
        rbuf->flush(ringbufferId);
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
