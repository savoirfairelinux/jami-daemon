/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ringbufferpool.h"
#include "ringbuffer.h"
#include "logger.h"

#include <cstring>

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
    std::lock_guard lk(stateLock_);

    if (sr != internalAudioFormat_.sample_rate) {
        flushAllBuffersLocked();
        internalAudioFormat_.sample_rate = sr;
    }
}

void
RingBufferPool::setInternalAudioFormat(AudioFormat format)
{
    std::lock_guard lk(stateLock_);

    if (format != internalAudioFormat_) {
        flushAllBuffersLocked();
        internalAudioFormat_ = format;
        for (auto& wrb : ringBufferMap_)
            if (auto rb = wrb.second.lock())
                rb->setFormat(internalAudioFormat_);
    }
}

std::shared_ptr<RingBuffer>
RingBufferPool::getRingBufferLocked(const std::string& id)
{
    const auto& it = ringBufferMap_.find(id);
    if (it != ringBufferMap_.cend()) {
        if (const auto& sptr = it->second.lock())
            return sptr;
        ringBufferMap_.erase(it);
    }

    return nullptr;
}

std::shared_ptr<RingBuffer>
RingBufferPool::getRingBufferLocked(const std::string& id) const
{
    const auto& it = ringBufferMap_.find(id);
    if (it != ringBufferMap_.cend())
        return it->second.lock();

    return nullptr;
}

std::shared_ptr<RingBuffer>
RingBufferPool::getRingBuffer(const std::string& id)
{
    std::lock_guard lk(stateLock_);
    return getRingBufferLocked(id);
}

std::shared_ptr<RingBuffer>
RingBufferPool::getRingBuffer(const std::string& id) const
{
    std::lock_guard lk(stateLock_);
    return getRingBufferLocked(id);
}

std::shared_ptr<RingBuffer>
RingBufferPool::createRingBuffer(const std::string& id)
{
    std::lock_guard lk(stateLock_);

    auto rbuf = getRingBufferLocked(id);
    if (rbuf) {
        JAMI_DEBUG("Ringbuffer already exists for id '{}'", id);
        return rbuf;
    }

    rbuf.reset(new RingBuffer(id, internalAudioFormat_));
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

void
RingBufferPool::addReaderToRingBuffer(const std::shared_ptr<RingBuffer>& sourceBuffer, const std::string& readerBufferId)
{
    if (readerBufferId != DEFAULT_ID and sourceBuffer->getId() == readerBufferId)
        JAMI_WARNING("RingBuffer has a readoffset on itself");

    sourceBuffer->createReadOffset(readerBufferId);
    readBindingsMap_[readerBufferId].insert(sourceBuffer);
}

void
RingBufferPool::removeReaderFromRingBuffer(const std::shared_ptr<RingBuffer>& sourceBuffer,
                                           const std::string& readerBufferId)
{
    if (auto* bindings = getReadBindings(readerBufferId)) {
        bindings->erase(sourceBuffer);
        if (bindings->empty())
            removeReadBindings(readerBufferId);
    }

    sourceBuffer->removeReadOffset(readerBufferId);
}

void
RingBufferPool::bindRingBuffers(const std::string& ringbufferId1, const std::string& ringbufferId2)
{
    JAMI_LOG("Bind ringbuffer {} to ringbuffer {}", ringbufferId1, ringbufferId2);

    std::lock_guard lk(stateLock_);

    const auto& rb1 = getRingBufferLocked(ringbufferId1);
    if (not rb1) {
        JAMI_ERROR("No ringbuffer associated with id '{}'", ringbufferId1);
        return;
    }

    const auto& rb2 = getRingBufferLocked(ringbufferId2);
    if (not rb2) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", ringbufferId2);
        return;
    }

    addReaderToRingBuffer(rb1, ringbufferId2);
    addReaderToRingBuffer(rb2, ringbufferId1);
}

void
RingBufferPool::bindHalfDuplexOut(const std::string& readerBufferId, const std::string& sourceBufferId)
{
    /* This method is used only for active ringbuffers, if this ringbuffer does not exist,
     * do nothing */
    std::lock_guard lk(stateLock_);

    if (const auto& rb = getRingBufferLocked(sourceBufferId)) {
        // p1 est le binding de p2 (p2 lit le stream de p1)
        addReaderToRingBuffer(rb, readerBufferId);
    }
}

void
RingBufferPool::unbindRingBuffers(const std::string& ringbufferId1, const std::string& ringbufferId2)
{
    JAMI_LOG("Unbind ringbuffers {} and {}", ringbufferId1, ringbufferId2);

    std::lock_guard lk(stateLock_);

    const auto& rb1 = getRingBufferLocked(ringbufferId1);
    if (not rb1) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", ringbufferId1);
        return;
    }

    const auto& rb2 = getRingBufferLocked(ringbufferId2);
    if (not rb2) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", ringbufferId2);
        return;
    }

    removeReaderFromRingBuffer(rb1, ringbufferId2);
    removeReaderFromRingBuffer(rb2, ringbufferId1);
}

void
RingBufferPool::unBindHalfDuplexOut(const std::string& readerBufferId, const std::string& sourceBufferId)
{
    std::lock_guard lk(stateLock_);

    if (const auto& rb = getRingBufferLocked(sourceBufferId))
        removeReaderFromRingBuffer(rb, readerBufferId);
}

void
RingBufferPool::unBindAllHalfDuplexOut(const std::string& ringbufferId)
{
    std::lock_guard lk(stateLock_);

    const auto& rb = getRingBufferLocked(ringbufferId);
    if (not rb) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", ringbufferId);
        return;
    }

    auto* bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return;
    const auto bindings_copy = *bindings; // temporary copy
    for (const auto& rbuf : bindings_copy) {
        removeReaderFromRingBuffer(rb, rbuf->getId());
    }
}

void
RingBufferPool::unBindAllHalfDuplexIn(const std::string& sourceBufferId)
{
    std::lock_guard lk(stateLock_);

    auto ringBuffer = getRingBufferLocked(sourceBufferId);
    if (not ringBuffer) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", sourceBufferId);
        return;
    }

    const std::vector<std::string>& subscribers = ringBuffer->getSubscribers();
    for (const auto& subscriber : subscribers) {
        removeReaderFromRingBuffer(ringBuffer, subscriber);
    }
}

void
RingBufferPool::unBindAll(const std::string& ringbufferId)
{
    JAMI_LOG("Unbind ringbuffer {} from all bound ringbuffers", ringbufferId);

    std::lock_guard lk(stateLock_);

    const auto& rb = getRingBufferLocked(ringbufferId);
    if (not rb) {
        JAMI_ERROR("No ringbuffer associated to id '{}'", ringbufferId);
        return;
    }

    auto* bindings = getReadBindings(ringbufferId);
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
    std::lock_guard lk(stateLock_);

    auto* const bindings = getReadBindings(ringbufferId);
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
RingBufferPool::waitForDataAvailable(const std::string& ringbufferId, const duration& max_wait) const
{
    return waitForDataAvailable(ringbufferId, clock::now() + max_wait);
}

bool
RingBufferPool::waitForDataAvailable(const std::string& ringbufferId, const time_point& deadline) const
{
    std::unique_lock lk(stateLock_);
    const auto* bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return false;
    const auto bindings_copy = *bindings; // temporary copy

    lk.unlock();
    for (const auto& rbuf : bindings_copy) {
        if (rbuf->waitForDataAvailable(ringbufferId, deadline) == 0)
            return false;
    }
    return true;
}

std::shared_ptr<AudioFrame>
RingBufferPool::getAvailableData(const std::string& ringbufferId)
{
    std::lock_guard lk(stateLock_);

    auto* bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return {};

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
    std::lock_guard lk(stateLock_);

    const auto* const bindings = getReadBindings(ringbufferId);
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
    std::lock_guard lk(stateLock_);

    auto* const bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return 0;

    for (const auto& rbuf : *bindings)
        rbuf->discard(toDiscard, ringbufferId);

    return toDiscard;
}

void
RingBufferPool::flush(const std::string& ringbufferId)
{
    std::lock_guard lk(stateLock_);

    auto* const bindings = getReadBindings(ringbufferId);
    if (not bindings)
        return;

    for (const auto& rbuf : *bindings)
        rbuf->flush(ringbufferId);
}

void
RingBufferPool::flushAllBuffersLocked()
{
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

void
RingBufferPool::flushAllBuffers()
{
    std::lock_guard lk(stateLock_);
    flushAllBuffersLocked();
}

bool
RingBufferPool::isAudioMeterActive(const std::string& id)
{
    std::lock_guard lk(stateLock_);
    if (!id.empty()) {
        if (auto rb = getRingBufferLocked(id)) {
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
    std::lock_guard lk(stateLock_);
    if (!id.empty()) {
        if (auto rb = getRingBufferLocked(id)) {
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
