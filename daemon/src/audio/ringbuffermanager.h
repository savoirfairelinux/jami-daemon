/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
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

#ifndef MAIN_BUFFER_MANAGER_H_
#define MAIN_BUFFER_MANAGER_H_

#include "audiobuffer.h"
#include "noncopyable.h"

#include <map>
#include <set>
#include <string>
#include <mutex>
#include <memory>

class RingBuffer;

class RingBufferManager {

    public:
        static const char * const DEFAULT_ID;

        RingBufferManager();

        ~RingBufferManager();

        int getInternalSamplingRate() const {
            return internalAudioFormat_.sample_rate;
        }

        AudioFormat getInternalAudioFormat() const {
            return internalAudioFormat_;
        }

        void setInternalSamplingRate(unsigned sr);

        void setInternalAudioFormat(AudioFormat format);

        /**
         * Bind together two audio streams so taht a client will be able
         * to put and get data specifying its callid only.
         */
        void bindCallID(const std::string& call_id1, const std::string& call_id2);

        /**
         * Add a new call_id to unidirectional outgoing stream
         * \param call_id New call id to be added for this stream
         * \param process_id Process that require this stream
         */
        void bindHalfDuplexOut(const std::string& process_id, const std::string& call_id);

        /**
         * Unbind two calls
         */
        void unBindCallID(const std::string& call_id1, const std::string& call_id2);

        /**
         * Unbind a unidirectional stream
         */
        void unBindHalfDuplexOut(const std::string& process_id, const std::string& call_id);

        void unBindAll(const std::string& call_id);

        bool waitForDataAvailable(const std::string& call_id, size_t min_data_length, const std::chrono::microseconds& max_wait) const;

        size_t getData(AudioBuffer& buffer, const std::string& call_id);

        size_t getAvailableData(AudioBuffer& buffer, const std::string& call_id);

        size_t availableForGet(const std::string& call_id) const;

        size_t discard(size_t toDiscard, const std::string& call_id);

        void flush(const std::string& call_id);

        void flushAllBuffers();

        /**
         * Create a new ringbuffer with a default readoffset.
         * This class keeps a weak reference on returned pointer,
         * so the caller is responsible of the refered instance.
         */
        std::shared_ptr<RingBuffer> createRingBuffer(const std::string& id);

        /**
         * Obtain a shared pointer on a RingBuffer given by its ID.
         * If the ID doesn't match to any RingBuffer, the shared pointer is empty.
         * This non-const version flush internal weak ponter if the ID was used and
         * the associated RingBuffer has been deleted.
         */
        std::shared_ptr<RingBuffer> getRingBuffer(const std::string& id);

        /**
         * Works as non-const getRingBuffer, without the weak reference flush.
         */
        std::shared_ptr<RingBuffer> getRingBuffer(const std::string& id) const;

    private:
        NON_COPYABLE(RingBufferManager);

        // A set of ring buffer readable by a call
        typedef std::set<std::shared_ptr<RingBuffer>, std::owner_less<std::shared_ptr<RingBuffer>> > ReadBindings;

        const RingBufferManager::ReadBindings* getReadBindings(const std::string& call_id) const;

        RingBufferManager::ReadBindings* getReadBindings(const std::string& call_id);

        void removeReadBindings(const std::string& call_id);

        void addReaderToRingBuffer(std::shared_ptr<RingBuffer> rbuf,
                                   const std::string& call_id);

        void removeReaderFromRingBuffer(std::shared_ptr<RingBuffer> rbuf,
                                        const std::string& call_id);

        // A cache of created RingBuffers listed by IDs.
        std::map<std::string, std::weak_ptr<RingBuffer> > ringBufferMap_{};

        // A map of which RingBuffers a call has some ReadOffsets
        std::map<std::string, ReadBindings> readBindingsMap_{};

        mutable std::recursive_mutex stateLock_{};

        AudioFormat internalAudioFormat_{AudioFormat::MONO()};

        std::shared_ptr<RingBuffer> defaultRingBuffer{};
};

#endif  // RingBufferManager
