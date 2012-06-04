/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __RING_BUFFER__
#define __RING_BUFFER__

#include <fstream>
#include <vector>
#include <map>
#include "noncopyable.h"

typedef std::map<std::string, size_t> ReadPointer;

class RingBuffer {
    public:
        /**
         * Constructor
         * @param size  Size of the buffer to create
         */
        RingBuffer(size_t size, const std::string &call_id);

        std::string getBufferId() const {
            return buffer_id_;
        }

        /**
         * Reset the counters to 0 for this read pointer
         */
        void flush(const std::string &call_id);

        void flushAll();

        /**
         * Get read pointer coresponding to this call
         */
        size_t getReadPointer(const std::string &call_id) const;

        /**
         * Get the whole readpointer list for this ringbuffer
         */
        ReadPointer* getReadPointerList() {
            return &readpointers_;
        }

        /**
         * Return the smalest readpointer. Usefull to evaluate if ringbuffer is full
         */
        size_t getSmallestReadPointer() const;

        /**
         * Move readpointer forward by pointer_value
         */
        void storeReadPointer(size_t pointer_value, const std::string &call_id);

        /**
         * Add a new readpointer for this ringbuffer
         */
        void createReadPointer(const std::string &call_id);

        /**
         * Remove a readpointer for this ringbuffer
         */
        void removeReadPointer(const std::string &call_id);

        /**
         * Test if readpointer coresponding to this call is still active
         */
        bool hasThisReadPointer(const std::string &call_id) const;

        bool hasNoReadPointers() const;

        /**
         * Write data in the ring buffer
         * @param buffer Data to copied
         * @param toCopy Number of bytes to copy
         */
        void put(void* buffer, size_t toCopy);

        /**
         * To get how much space is available in the buffer to read in
         * @return int The available size
         */
        size_t availableForGet(const std::string &call_id) const;

        /**
         * Get data in the ring buffer
         * @param buffer Data to copied
         * @param toCopy Number of bytes to copy
         * @return size_t Number of bytes copied
         */
        size_t get(void* buffer, size_t toCopy, const std::string &call_id);

        /**
         * Discard data from the buffer
         * @param toDiscard Number of bytes to discard
         * @return size_t Number of bytes discarded
         */
        size_t discard(size_t toDiscard, const std::string &call_id);

        /**
         * Total length of the ring buffer which is available for "putting"
         * @return int
         */
        size_t putLength() const;

        size_t getLength(const std::string &call_id) const;

        /**
         * Debug function print mEnd, mStart, mBufferSize
         */
        void debug();

    private:
        NON_COPYABLE(RingBuffer);

        /** Pointer on the last data */
        size_t endPos_;
        /** Buffer size */
        size_t bufferSize_;
        /** Data */
        std::vector<unsigned char> buffer_;

        ReadPointer readpointers_;
        std::string buffer_id_;

        friend class MainBufferTest;
};


#endif /*  __RING_BUFFER__ */
