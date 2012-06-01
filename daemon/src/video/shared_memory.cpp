/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include "shared_memory.h"

// shm includes
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>     /* semaphore functions and structs.    */
#include <sys/shm.h>

#include <cstdlib>
#include <stdexcept>

#include "manager.h"
#include "logger.h"
#include "dbus/video_controls.h"
#include "fileutils.h"

namespace sfl_video {

namespace { // anonymous namespace

#if _SEM_SEMUN_UNDEFINED
union semun {
 int val;				    /* value for SETVAL */
 struct semid_ds *buf;		/* buffer for IPC_STAT & IPC_SET */
 unsigned short int *array;	/* array for GETALL & SETALL */
 struct seminfo *__buf;		/* buffer for IPC_INFO */
};
#endif

void cleanupSemaphore(int semaphoreSetID)
{
    semctl(semaphoreSetID, 0, IPC_RMID);
}

/*
 * function: sem_signal. signals the process that a frame is ready.
 * input:    semaphore set ID.
 * output:   none.
 */
void sem_signal(int semaphoreSetID)
{
    /* structure for semaphore operations.   */
    sembuf sem_op;

    /* signal the semaphore - increase its value by one. */
    sem_op.sem_num = 0;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    semop(semaphoreSetID, &sem_op, 1);
}

/* join and/or create a shared memory segment */
int createShmKey()
{
    /* connect to and possibly create a segment with 644 permissions
       (rw-r--r--) */
    srand(time(NULL));
    int proj_id = rand();
    return ftok(fileutils::get_program_dir(), proj_id);
}

int createShmID(int key, int numBytes)
{
    int shm_id = shmget(key, numBytes, 0644 | IPC_CREAT);

    if (shm_id == -1)
        ERROR("%s:shmget:%m", __PRETTY_FUNCTION__);

    return shm_id;
}

/* attach a shared memory segment */
uint8_t *attachShm(int shm_id)
{
    /* attach to the segment and get a pointer to it */
    uint8_t *data = reinterpret_cast<uint8_t*>(shmat(shm_id, (void *) 0, 0));
    if (data == reinterpret_cast<uint8_t *>(-1)) {
        ERROR("%s:shmat:%m", __PRETTY_FUNCTION__);
        data = NULL;
    }
    return data;
}

void detachShm(uint8_t *data)
{
    /* detach from the segment: */
    if (data and shmdt(data) == -1)
        ERROR("%s:shmdt:%m", __PRETTY_FUNCTION__);
}

void destroyShm(int shm_id)
{
    /* destroy it */
    shmctl(shm_id, IPC_RMID, NULL);
}

void cleanupShm(int shm_id, uint8_t *data)
{
    detachShm(data);
    destroyShm(shm_id);
}

int createSemaphoreKey(int shmKey)
{
    key_t key;
    do
		key = ftok(fileutils::get_program_dir(), rand());
    while (key == shmKey);
    return key;
}

int createSemaphoreSetID(int semaphoreKey)
{
    /* first we create a semaphore set with a single semaphore,
       whose counter is initialized to '0'. */
    int semaphoreSetID = semget(semaphoreKey, 1, 0600 | IPC_CREAT);
    if (semaphoreSetID == -1) {
        ERROR("%s:semget:%m", __PRETTY_FUNCTION__);
        throw std::runtime_error("Could not create semaphore set");
    }

    /* semaphore value, for semctl(). */
    union semun sem_val;
    sem_val.val = 0;
    semctl(semaphoreSetID, 0, SETVAL, sem_val);
    return semaphoreSetID;
}
} // end anonymous namespace

SharedMemory::SharedMemory(VideoControls &controls) :
    videoControls_(controls),
    shmKey_(0),
    shmID_(0),
    shmBuffer_(0),
    semaphoreSetID_(0),
    semaphoreKey_(0),
    dstWidth_(0),
    dstHeight_(0),
    bufferSize_(0),
    shmReady_()
{}

void SharedMemory::allocateBuffer(int width, int height, int size)
{
    dstWidth_ = width;
    dstHeight_ = height;
    bufferSize_ = size;
    shmKey_ = createShmKey();
    shmID_ = createShmID(shmKey_, bufferSize_);
    shmBuffer_ = attachShm(shmID_);
    semaphoreKey_ = createSemaphoreKey(shmKey_);
    semaphoreSetID_ = createSemaphoreSetID(semaphoreKey_);
    shmReady_.signal();
}

void SharedMemory::publishShm()
{
    DEBUG("Publishing shm: %d sem: %d size: %d", shmKey_, semaphoreKey_,
          bufferSize_);
    videoControls_.receivingEvent(shmKey_, semaphoreKey_, bufferSize_,
                                  dstWidth_, dstHeight_);
}

void SharedMemory::waitForShm()
{
    shmReady_.wait();
}

void SharedMemory::frameUpdatedCallback()
{
    // signal the semaphore that a new frame is ready
    sem_signal(semaphoreSetID_);
}

SharedMemory::~SharedMemory()
{
    // free shared memory resources
	videoControls_.stoppedReceivingEvent(shmKey_, semaphoreKey_);

    // make sure no one is waiting for the SHM event which will never come if we've error'd out
    shmReady_.signal();

    cleanupSemaphore(semaphoreSetID_);
    cleanupShm(shmID_, shmBuffer_);
}
} // end namespace sfl_video
