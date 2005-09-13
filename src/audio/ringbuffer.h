/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 * 
 *  Portions Copyright (C) Dominic Mazzoni (Audacity)
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#include <cc++/thread.h>

#include "../global.h"


typedef unsigned char* 	samplePtr;

// template <typename T>
class RingBuffer {
 public:
   RingBuffer(int size);
   ~RingBuffer();

   // To set counters to 0
   void flush (void);
   
   //
   // For the writer only:
   //
   int AvailForPut (void);
   int Put (void*, int);

   //
   // For the reader only:
   //
   int AvailForGet (void);
   int Get (void *, int);
   int Discard(int);

   int Len();
   
 private:
 //  T getNextSample(void);

   void lock (void);
   void unlock (void);

   ost::Mutex  		 mMutex;
   int           mStart;
   int           mEnd;
   int           mBufferSize;
   samplePtr     mBuffer;
};

#endif /*  __RING_BUFFER__ */


