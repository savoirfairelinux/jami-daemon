/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
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


#include <iostream>  //debug
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
   mBuffer = (samplePtr) malloc (mBufferSize);
   assert (mBuffer != NULL);
}

// Free memory on object deletion
RingBuffer::~RingBuffer() {
   free (mBuffer);
}
 
void
RingBuffer::flush (void) {
	mMutex.enterMutex();
	mStart = 0; 
	mEnd = 0;
	mMutex.leaveMutex();
} 

int 
RingBuffer::Len() { 
   return (mEnd + mBufferSize - mStart) % mBufferSize;
}
 
//
// For the writer only:
//
int 
RingBuffer::AvailForPut() {
   return (mBufferSize-4) - Len();
} 

// This one puts some data inside the ring buffer.
int 
RingBuffer::Put(void* buffer, int toCopy) {
   samplePtr src;
   int block;
   int copied;
   int pos;
   int len = Len();
  
   mMutex.enterMutex();
   if (toCopy > (mBufferSize-4) - len)
      toCopy = (mBufferSize-4) - len; 

   src = (samplePtr) buffer; 
   
   copied = 0;
   pos = mEnd;

   while(toCopy) {
      block = toCopy;
      if (block > mBufferSize - pos) // from current pos. to end of buffer
         block = mBufferSize - pos; 

	  // put the data inside the buffer.
	  bcopy (src, mBuffer + pos, block);
      
      src += block;
      pos = (pos + block) % mBufferSize;
      toCopy -= block;
      copied += block;
   }

   mEnd = pos;
   
   mMutex.leaveMutex();

   // How many items copied.
   return copied;
}

//
// For the reader only:
//

int 
RingBuffer::AvailForGet() {
	// Used space
   return Len();
}

// Get will move 'toCopy' bytes from the internal FIFO to 'buffer'
int 
RingBuffer::Get(void *buffer, int toCopy) {
   samplePtr dest;
   int block;
   int copied;
   int len = Len();
	
   mMutex.enterMutex();
   if (toCopy > len)
      toCopy = len;

   dest = (samplePtr) buffer;
   copied = 0;
   
   while(toCopy) {
      block = toCopy;
      if (block > mBufferSize - mStart)
         block = mBufferSize - mStart;

      bcopy (mBuffer + mStart, dest, block);
      dest += block;
      mStart = (mStart + block) % mBufferSize;
      toCopy -= block;
      copied += block;
   }
	
   mMutex.leaveMutex();
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

/*
T
RingBuffer::getNextItem (void) {
   return (T) 0;
}
*/
