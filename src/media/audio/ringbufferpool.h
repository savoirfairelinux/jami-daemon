/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#pragma once

#include "audio_format.h"
#include "media_buffer.h"
#include "noncopyable.h"

#include <map>
#include <set>
#include <string>
#include <mutex>
#include <memory>

namespace jami {

class RingBuffer;

class RingBufferPool
{
public:
    static const char* const DEFAULT_ID;

    RingBufferPool();
    ~RingBufferPool();

    int getInternalSamplingRate() const { return internalAudioFormat_.sample_rate; }

    AudioFormat getInternalAudioFormat() const { return internalAudioFormat_; }

    void setInternalSamplingRate(unsigned sr);

    void setInternalAudioFormat(AudioFormat format);

    /**
     * Bind together two audio streams
     */
    void bindRingbuffers(const std::string& ringbufferId1, const std::string& ringbufferId2);

    /**
     * Add a new ringbufferId to unidirectional outgoing stream
     * \param ringbufferId New ringbufferId to be added for this stream
     * \param processId Process that require this stream
     */
    void bindHalfDuplexOut(const std::string& processId, const std::string& ringbufferId);

    /**
     * Unbind two ringbuffers
     */
    void unbindRingbuffers(const std::string& ringbufferId1, const std::string& ringbufferId2);

    /**
     * Unbind a unidirectional stream
     */
    void unBindHalfDuplexOut(const std::string& process_id, const std::string& ringbufferId);

    void unBindAllHalfDuplexOut(const std::string& ringbufferId);

    void unBindAll(const std::string& ringbufferId);

    bool waitForDataAvailable(const std::string& ringbufferId,
                              const std::chrono::microseconds& max_wait) const;

    std::shared_ptr<AudioFrame> getData(const std::string& ringbufferId);

    std::shared_ptr<AudioFrame> getAvailableData(const std::string& ringbufferId);

    size_t availableForGet(const std::string& ringbufferId) const;

    size_t discard(size_t toDiscard, const std::string& ringbufferId);

    void flush(const std::string& ringbufferId);

    void flushAllBuffers();

    /**
     * Create a new ringbuffer with a default readoffset.
     * This class keeps a weak reference on returned pointer,
     * so the caller is responsible of the referred instance.
     */
    std::shared_ptr<RingBuffer> createRingBuffer(const std::string& id);

    /**
     * Obtain a shared pointer on a RingBuffer given by its ID.
     * If the ID doesn't match to any RingBuffer, the shared pointer
     * is empty. This non-const version flushes internal weak pointers
     * if the ID was used and the associated RingBuffer has been deleted.
     */
    std::shared_ptr<RingBuffer> getRingBuffer(const std::string& id);

    /**
     * Works as non-const getRingBuffer, without the weak reference flush.
     */
    std::shared_ptr<RingBuffer> getRingBuffer(const std::string& id) const;

    bool isAudioMeterActive(const std::string& id);
    void setAudioMeterState(const std::string& id, bool state);

private:
    NON_COPYABLE(RingBufferPool);

    // A set of RingBuffers readable by a call
    using ReadBindings
        = std::set<std::shared_ptr<RingBuffer>, std::owner_less<std::shared_ptr<RingBuffer>>>;

    const ReadBindings* getReadBindings(const std::string& ringbufferId) const;
    ReadBindings* getReadBindings(const std::string& ringbufferId);

    void removeReadBindings(const std::string& ringbufferId);

    void addReaderToRingBuffer(const std::shared_ptr<RingBuffer>& rbuf, const std::string& ringbufferId);

    void removeReaderFromRingBuffer(const std::shared_ptr<RingBuffer>& rbuf,
                                    const std::string& ringbufferId);

    // A cache of created RingBuffers listed by IDs.
    std::map<std::string, std::weak_ptr<RingBuffer>> ringBufferMap_ {};

    // A map of which RingBuffers a call has some ReadOffsets
    std::map<std::string, ReadBindings> readBindingsMap_ {};

    mutable std::recursive_mutex stateLock_ {};

    AudioFormat internalAudioFormat_ {AudioFormat::DEFAULT()};

    std::shared_ptr<RingBuffer> defaultRingBuffer_;
};

} // namespace jami
