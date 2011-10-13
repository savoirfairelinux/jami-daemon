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

#include "../call.h"

#include <fstream>

typedef std::map<std::string, int> ReadPointer;

class RingBuffer {
    public:
        /**
         * Constructor
         * @param size  Size of the buffer to create
         */
        RingBuffer(int size, const std::string &call_id = Call::DEFAULT_ID);

        /**
         * Destructor
         */
        ~RingBuffer();

        std::string getBufferId() {
            return buffer_id_;
        }

        /**
         * Reset the counters to 0 for this read pointer
         */
        void flush(const std::string &call_id = Call::DEFAULT_ID);

        void flushAll();

        /**
         * Get read pointer coresponding to this call
         */
        int getReadPointer(const std::string &call_id = Call::DEFAULT_ID);

        /**
         * Get the whole readpointer list for this ringbuffer
         */
        ReadPointer* getReadPointerList() {
            return &readpointer_;
        }

        /**
         * Return the smalest readpointer. Usefull to evaluate if ringbuffer is full
         */
        int getSmallestReadPointer();

        /**
         * Move readpointer forward by pointer_value
         */
        void storeReadPointer(int pointer_value, const std::string &call_id = Call::DEFAULT_ID);

        /**
         * Add a new readpointer for this ringbuffer
         */
        void createReadPointer(const std::string &call_id = Call::DEFAULT_ID);

        /**
         * Remove a readpointer for this ringbuffer
         */
        void removeReadPointer(const std::string &call_id = Call::DEFAULT_ID);

        /**
         * Test if readpointer coresponding to this call is still active
         */
        bool hasThisReadPointer(const std::string &call_id);

        int getNbReadPointer();

        /**
         * Write data in the ring buffer
         * @param buffer Data to copied
         * @param toCopy Number of bytes to copy
         */
        void Put(void* buffer, int toCopy);

        /**
         * To get how much space is available in the buffer to read in
         * @return int The available size
         */
        int AvailForGet(const std::string &call_id = Call::DEFAULT_ID);

        /**
         * Get data in the ring buffer
         * @param buffer Data to copied
         * @param toCopy Number of bytes to copy
         * @return int Number of bytes copied
         */
        int Get(void* buffer, int toCopy, const std::string &call_id = Call::DEFAULT_ID);

        /**
         * Discard data from the buffer
         * @param toDiscard Number of bytes to discard
         * @return int Number of bytes discarded
         */
        int Discard(int toDiscard, const std::string &call_id = Call::DEFAULT_ID);

        /**
         * Total length of the ring buffer
         * @return int
         */
        int putLen();

        int getLen(const std::string &call_id = Call::DEFAULT_ID);

        /**
         * Debug function print mEnd, mStart, mBufferSize
         */
        void debug();

    private:
        // Copy Constructor
        RingBuffer(const RingBuffer& rh);

        // Assignment operator
        RingBuffer& operator= (const RingBuffer& rh);

        /** Pointer on the last data */
        int           endPos_;
        /** Buffer size */
        int           bufferSize_;
        /** Data */
        unsigned char *buffer_;

        ReadPointer   readpointer_;
        std::string buffer_id_;

    public:

        friend class MainBufferTest;

        std::fstream *buffer_input_rec;
        std::fstream *buffer_output_rec;

        static int count_rb;
};


#endif /*  __RING_BUFFER__ */
