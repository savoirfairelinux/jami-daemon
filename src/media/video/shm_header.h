/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#ifndef SHM_HEADER_H_
#define SHM_HEADER_H_

#include <cstdint>
#include <semaphore.h>

/* Implementation note: double-buffering
 * Shared memory is divided in two regions, each representing one frame.
 * First byte of each frame is guaranteed to be aligned on 16 bytes.
 * One region is marked as readable: this region can be safely read.
 * The other region is writeable: only the producer can use it.
 */

struct SHMHeader
{
    sem_t mutex;          // lock it before any operations on these fields
    sem_t frameGenMutex;  // unlocked by producer when frameGen is modified
    unsigned frameGen;    // monotonically incremented when a producer changes readOffset
    unsigned frameSize;   // size in bytes of 1 frame
    unsigned mapSize;     // size to map if you need all the data
    unsigned readOffset;  // offset of readable frame in data
    unsigned writeOffset; // offset of writable frame in data
    uint8_t data[];       // the whole shared memory
};

#endif
