/**********************************************************************
  Portions (c) Dominic Mazzoni (Audacity)

  This class is thread-safe, assuming that there is only one
  thread writing, and one thread reading.  If two threads both
  need to read, or both need to write, they need to lock this
  class from outside using their own mutex.

  AvailForPut and AvailForGet may underestimate but will never
  overestimate.

**********************************************************************/
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
RingBuffer::lock (void) 
{
	mMutex.enterMutex();
} 
 
void
RingBuffer::unlock (void) 
{
	mMutex.leaveMutex();
}
 
void
RingBuffer::flush (void) {
	lock();
	mStart = 0; 
	mEnd = 0;
	unlock();
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
  
   lock();
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
   
   unlock();

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
	
   lock();
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
	
   unlock();
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
