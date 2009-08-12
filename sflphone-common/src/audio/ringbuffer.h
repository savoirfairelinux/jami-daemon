/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 * 
 *  Portions Copyright (C) Dominic Mazzoni (Audacity)
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



typedef unsigned char* samplePtr;

typedef map<CallID, int> ReadPointer;

class RingBuffer {
  public:
    /**
     * Constructor
     * @param size  Size of the buffer to create
     */
    RingBuffer(int size);

    /**
     * Destructor
     */
    ~RingBuffer();


    /**
     * Reset the counters to 0
     */
    void flush (CallID call_id = default_id);


    int getReadPointer(CallID call_id = default_id);

    int getSmallestReadPointer();

    void storeReadPointer(int pointer_value, CallID call_id = default_id);

    void createReadPointer(CallID call_id = default_id);

    void removeReadPointer(CallID call_id = default_id);

    int getNbReadPointer();

    /**
     * To get how much space is available in the buffer to write in
     * @return int The available size
     */
    int AvailForPut (void);

    /**
     * Write data in the ring buffer
     * @param buffer Data to copied
     * @param toCopy Number of bytes to copy
     * @param volume The volume
     * @return int Number of bytes copied
     */
    int Put (void* buffer, int toCopy, unsigned short volume = 100);

    /**
     * To get how much space is available in the buffer to read in
     * @return int The available size
     */
    int AvailForGet (CallID call_id = default_id);

    /**
     * Get data in the ring buffer
     * @param buffer Data to copied
     * @param toCopy Number of bytes to copy
     * @param volume The volume
     * @return int Number of bytes copied
     */
    int Get (void* buffer, int toCopy, unsigned short volume = 100, CallID call_id = default_id);

    /**
     * Discard data from the buffer
     * @param toDiscard Number of bytes to discard
     * @return int Number of bytes discarded 
     */
    int Discard(int toDiscard, CallID call_id = default_id);

    /**
     * Total length of the ring buffer
     * @return int  
     */
    int putLen();

    int getLen(CallID call_id = default_id);
    
    /**
     * Debug function print mEnd, mStart, mBufferSize
     */
    void debug();

  private:
    // Copy Constructor
    RingBuffer(const RingBuffer& rh);

    // Assignment operator
    RingBuffer& operator=(const RingBuffer& rh);

    /** Pointer on the first data */
    int           mStart;
    /** Pointer on the last data */
    int           mEnd;
    /** Buffer size */
    int           mBufferSize;
    /** Data */
    samplePtr     mBuffer;

    ReadPointer   _readpointer;

  public:

    friend class MainBufferTest;
};

#endif /*  __RING_BUFFER__ */
