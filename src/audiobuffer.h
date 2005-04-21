/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
 *
 * 	Portions Copyright (c) 2000 Billy Biggs <bbiggs@div8.net>
 *  Portions Copyright (c) 2004 Wirlab <kphone@wirlab.net>
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
#ifndef __AUDIOBUFFER_H__
#define __AUDIOBUFFER_H__

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>


/**
 * Small class for passing around buffers of audio data.
 */
class AudioBuffer {
public:
	/**
	 * Creates an audio buffer of @param length bytes.
	 */
	AudioBuffer (void);

	/**
	 * Deletes the audio buffer, freeing the data.
	 */
	~AudioBuffer (void);

	/**
	 * Returns a pointer to the audio data.
	 */
	void *getData (void) {
		return data;
	}

	/**
	 * Returns the size of the buffer.
	 */
	size_t getSize (void) {
	   return size; 
	}

	/**
	 * Resizes the buffer to size newlength. Will only allocate new memory
	 * if the size is larger than what has been previously allocated.
	 */
	void resize (size_t newsize);

	void setData (short *buf, int);
void *data;
private:
	
	size_t realsize;
	size_t size;
};

#endif // __AUDIOBUFFER_H__
