/**********************************************************************

  Audacity: A Digital Audio Editor

  RingBuffer.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __RING_BUFFER__
#define __RING_BUFFER__

#include <cc++/thread.h>

#include "../global.h"

using namespace ost;

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

   Mutex  		 mMutex;
   int           mStart;
   int           mEnd;
   int           mBufferSize;
   samplePtr     mBuffer;
};

#endif /*  __RING_BUFFER__ */


