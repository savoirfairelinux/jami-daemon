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

#ifndef MAIN_BUFFER_H_
#define MAIN_BUFFER_H_

#include "audiobuffer.h"
#include "noncopyable.h"

#include <map>
#include <set>
#include <string>
#include <mutex>
#include <memory>

class RingBuffer;

typedef std::set<std::string> CallIDSet;

class MainBuffer {

    public:
        static const char * const DEFAULT_ID;

        MainBuffer();

        ~MainBuffer();

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

        void putData(AudioBuffer& buffer, const std::string& call_id);

        bool waitForDataAvailable(const std::string& call_id, size_t min_data_length, const std::chrono::microseconds& max_wait) const;

        size_t getData(AudioBuffer& buffer, const std::string& call_id);
        size_t getAvailableData(AudioBuffer& buffer, const std::string& call_id);

        size_t availableForGet(const std::string& call_id) const;

        size_t discard(size_t toDiscard, const std::string& call_id);

        void flush(const std::string& call_id);

        void flushAllBuffers();

    private:
        NON_COPYABLE(MainBuffer);

        bool hasCallIDSet(const std::string& call_id);
        std::shared_ptr<CallIDSet> getCallIDSet(const std::string& call_id);
        std::shared_ptr<CallIDSet> getCallIDSet(const std::string& call_id) const;

        void createCallIDSet(const std::string& set_id);

        void removeCallIDSet(const std::string& set_id);

        /**
         * Add a new call id to this set
         */
        void addCallIDtoSet(const std::string& set_id, const std::string& call_id);

        void removeCallIDfromSet(const std::string& set_id, const std::string& call_id);

        /**
         * Create a new ringbuffer with default readoffset
         */
        void createRingBuffer(const std::string& call_id);

        void removeRingBuffer(const std::string& call_id);

        bool hasRingBuffer(const std::string& call_id);
        std::shared_ptr<RingBuffer> getRingBuffer(const std::string& call_id);
        std::shared_ptr<RingBuffer> getRingBuffer(const std::string& call_id) const;

        void removeReadOffsetFromRingBuffer(const std::string& call_id1,
                                             const std::string& call_id2);

        size_t getDataByID(AudioBuffer& buffer, const std::string& call_id, const std::string& reader_id);

        size_t availableForGetByID(const std::string& call_id, const std::string& reader_id) const;

        void discardByID(size_t toDiscard, const std::string& call_id, const std::string& reader_id);

        void flushByID(const std::string& call_id, const std::string& reader_id);

        typedef std::map<std::string, std::shared_ptr<RingBuffer> > RingBufferMap;
        RingBufferMap ringBufferMap_ = RingBufferMap{};

        typedef std::map<std::string, std::shared_ptr<CallIDSet> > CallIDMap;
        CallIDMap callIDMap_ = CallIDMap{};

        mutable std::recursive_mutex stateLock_ = {};

        AudioFormat internalAudioFormat_ = AudioFormat::MONO;
};

#endif  // MainBuffer
