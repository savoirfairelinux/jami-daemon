/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
 *
 *  Portions (c) Dominic Mazzoni (Audacity)
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

#include <assert.h>
#include <stdlib.h> 
#include <string.h>  
 
#include "ringbuffer.h"
#include "../global.h"
 
#define MIN_BUFFER_SIZE	1280

// Create  a ring buffer with 'size' bytes
RingBuffer::RingBuffer(int size) {
   mBufferSize = (size > MIN_BUFFER_SIZE ? size : MIN_BUFFER_SIZE);
   mStart = 0;
   mEnd = 0;
   mBuffer = new unsigned char[mBufferSize];
   assert (mBuffer != NULL);
}

// Free memory on object deletion
RingBuffer::~RingBuffer() {
   delete[] mBuffer;  mBuffer = NULL;
}
 
void
RingBuffer::flush (void) {
	mStart = 0; 
	mEnd = 0;
} 

int 
RingBuffer::Len() const { 
   return (mEnd + mBufferSize - mStart) % mBufferSize;
}
 
//
// For the writer only:
//
int 
RingBuffer::AvailForPut() const {
   return (mBufferSize-4) - Len();
} 

// This one puts some data inside the ring buffer.
// Change the volume if it's not 100
int 
RingBuffer::Put(void* buffer, int toCopy, unsigned short volume) {
   samplePtr src;
   int block;
   int copied;
   int pos;
   int len = Len();

   if (toCopy > (mBufferSize-4) - len)
      toCopy = (mBufferSize-4) - len;

   src = (samplePtr) buffer;

   copied = 0;
   pos = mEnd;

   while(toCopy) {
      block = toCopy;
      if (block > (mBufferSize - pos)) // from current pos. to end of buffer
         block = mBufferSize - pos;

      // put the data inside the buffer.
      if (volume!=100) {
        int16* src16 = (int16*)src;
        int int16len = (block >> 1);
        for (int i=0; i < int16len; i++) { src16[i] = src16[i] * volume / 100; }
      }
      // bcopy(src, dest, len)
      //fprintf(stderr, "has %d put %d\t", len, block);
      bcopy (src, mBuffer + pos, block);
      src += block;
      pos = (pos + block) % mBufferSize;
      toCopy -= block;
      copied += block;
   }

   mEnd = pos;

   // How many items copied.
   return copied;
}

//
// For the reader only:
//

int 
RingBuffer::AvailForGet() const {
	// Used space
   return Len();
}

// Get will move 'toCopy' bytes from the internal FIFO to 'buffer'
int 
RingBuffer::Get(void *buffer, int toCopy, unsigned short volume) {
   samplePtr dest;
   int block;
   int copied;
   int len = Len();

   if (toCopy > len)
      toCopy = len;

   dest = (samplePtr) buffer;
   copied = 0;

   //fprintf(stderr, "get start... ");
   while(toCopy) {
      block = toCopy;
      if (block > (mBufferSize - mStart))
         block = mBufferSize - mStart;

      if(volume!=100) {
        int16* start = (int16*)(mBuffer + mStart);
        int int16len = (block >> 1);
        for (int i=0; i<int16len; i++) { start[i] = start[i] * volume / 100; }
      }
      // bcopy(src, dest, len)
      bcopy (mBuffer + mStart, dest, block);
      dest += block;
      mStart = (mStart + block) % mBufferSize;
      toCopy -= block;
      copied += block;
   }
	
   return copied;
}

// Used to discard some bytes.
int 
RingBuffer::Discard(int toDiscard) {
   int len = Len();

   if (toDiscard > len)
      toDiscard = len;

   mStart = (mStart + toDiscard) % mBufferSize;

   return toDiscard;
}
