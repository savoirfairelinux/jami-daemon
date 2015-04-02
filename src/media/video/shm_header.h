/*
 *  Copyright (C) 2012-2015 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *
 *  Portions derived from GStreamer:
 *  Copyright (C) <2009> Collabora Ltd
 *  @author: Olivier Crete <olivier.crete@collabora.co.uk
 *  Copyright (C) <2009> Nokia Inc
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

#ifndef SHM_HEADER_H_
#define SHM_HEADER_H_

#include <semaphore.h>

// Implementation note: double-buffering
// Shared memory is divided in two regions, each representing one frame.
// First byte of each frame is warranted to by aligned on 16 bytes.
// One region is marked readable: this region can be safely read.
// The other region is writeable: only the producer can use it.

struct SHMHeader {
    sem_t mutex; // Lock it before any operations on following fields.
    sem_t frameGenMutex; // unlocked by producer when frameGen modified
    unsigned frameGen; // monotonically incremented when a producer changes readOffset
    unsigned frameSize; // size in bytes of 1 frame
    unsigned readOffset; // offset of readable frame in data
    unsigned writeOffset; // offset of writable frame in data
    char data[]; // the whole shared memory
};

#endif
