/*
 *  Copyright (C) 2011, 2012 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef SHARED_MEMORY_H_
#define SHARED_MEMORY_H_

#include <cc++/thread.h>
#include "noncopyable.h"

namespace sfl_video {
class SharedMemory {
    private:
        NON_COPYABLE(SharedMemory);

        /*-------------------------------------------------------------*/
        /* These variables should be used in thread (i.e. run()) only! */
        /*-------------------------------------------------------------*/
        int bufferSize_;
        int shmKey_;
        int shmID_;
        uint8_t *shmBuffer_;
        int semaphoreSetID_;
        int semaphoreKey_;

        int dstWidth_;
        int dstHeight_;
        ost::Event shmReady_;

    public:
        void frameUpdatedCallback();
        void waitForShm();
        // Returns a pointer to the memory where frames should be copied
        void *getTargetBuffer();
        int getShmKey() const { return shmKey_; }
        int getSemaphoreKey() const { return sempahoreKey_; }
        int getBufferSize() const { return bufferSize_; }
};
}

#endif // SHARED_MEMORY_H_
