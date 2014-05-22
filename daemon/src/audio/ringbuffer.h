/*
 *  Copyright (C) 2007-2012 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Adrien Beraud <adrien.beraud@gmail.com>
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

#ifndef __RING_BUFFER__
#define __RING_BUFFER__

#include "audiobuffer.h"
#include "noncopyable.h"

#include <condition_variable>
#include <mutex>
#include <chrono>
#include <map>
#include <vector>
#include <fstream>

typedef std::map<std::string, size_t> ReadOffset;

/**
 * A ring buffer for mutichannel audio samples
 */
class RingBuffer {
    public:
        /**
         * Constructor
         * @param size  Size of the buffer to create
         */
        RingBuffer(size_t size, const std::string &call_id, AudioFormat format=AudioFormat::MONO);

        std::string getBufferId() const {
            return buffer_id_;
        }

        /**
         * Reset the counters to 0 for this read offset
         */
        void flush(const std::string &call_id);

        void flushAll();

        inline  AudioFormat getFormat() const {
            return buffer_.getFormat();
        }

        inline void setFormat(AudioFormat format) {
            buffer_.setFormat(format);
        }

        /**
         * Add a new readoffset for this ringbuffer
         */
        void createReadOffset(const std::string &call_id);

        /**
         * Remove a readoffset for this ringbuffer
         */
        void removeReadOffset(const std::string &call_id);

        size_t readOffsetCount() const { return readoffsets_.size(); }

        bool hasNoReadOffsets() const;

        /**
         * Write data in the ring buffer
         * @param buffer Data to copied
         * @param toCopy Number of bytes to copy
         */
         void put(AudioBuffer& buf);

        /**
         * To get how much samples are available in the buffer to read in
         * @return int The available (multichannel) samples number
         */
        size_t availableForGet(const std::string &call_id) const;

        /**
         * Get data in the ring buffer
         * @param buffer Data to copied
         * @param toCopy Number of bytes to copy
         * @return size_t Number of bytes copied
         */
         size_t get(AudioBuffer& buf, const std::string &call_id);

        /**
         * Discard data from the buffer
         * @param toDiscard Number of samples to discard
         * @return size_t Number of samples discarded
         */
        size_t discard(size_t toDiscard, const std::string &call_id);

        /**
         * Total length of the ring buffer which is available for "putting"
         * @return int
         */
        size_t putLength() const;

        size_t getLength(const std::string &call_id) const;

        inline bool isFull() const {
            return putLength() == buffer_.frames();
        }

        inline bool isEmpty() const {
            return putLength() == 0;
        }


        /**
         * Blocks until min_data_length samples of data is available, or until deadline is missed.
         *
         * @param call_id The read offset for which data should be available.
         * @param min_data_length Minimum number of samples that should be vailable for the call to return
         * @param deadline The call is garenteed to end after this time point. If no deadline is provided, the the call blocks indefinitely.
         * @return available data for call_id after the call returned (same as calling getLength(call_id) ).
         */
        size_t waitForDataAvailable(const std::string &call_id, const size_t min_data_length, const std::chrono::high_resolution_clock::time_point& deadline) const;

        /**
         * Debug function print mEnd, mStart, mBufferSize
         */
        void debug();

    private:
        NON_COPYABLE(RingBuffer);

        /**
         * Return the smalest readoffset. Usefull to evaluate if ringbuffer is full
         */
        size_t getSmallestReadOffset() const;

        /**
         * Get read offset coresponding to this call
         */
        size_t getReadOffset(const std::string &call_id) const;

        /**
         * Move readoffset forward by offset
         */
        void storeReadOffset(size_t offset, const std::string &call_id);

        /**
         * Test if readoffset coresponding to this call is still active
         */
        bool hasThisReadOffset(const std::string &call_id) const;

        /**
         * Discard data from all read offsets to make place for new data.
         */
        size_t discard(size_t toDiscard);

        /** Offset on the last data */
        size_t endPos_;
        /** Data */
        AudioBuffer buffer_;

        mutable std::mutex lock_;
        mutable std::condition_variable not_empty_;

        ReadOffset readoffsets_;
        std::string buffer_id_;
};


#endif /*  __RING_BUFFER__ */
