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

#include "audiobuffer.h"
#include "global.h"

#include <string.h>

AudioBuffer::AudioBuffer (void) {
	data = new short[SIZEBUF];
	bzero(data, SIZEBUF);
	size = SIZEBUF;
	realsize = size;
}

AudioBuffer::~AudioBuffer (void)
{
	delete[] static_cast<short *>(data);
}

void AudioBuffer::resize (size_t newsize)
{
	if (newsize > realsize) {
		delete[] static_cast<short *>(data);
		data = new short[newsize];
		size = newsize;
		realsize = newsize;
	} else {
		size = newsize;
	}
}

void
AudioBuffer::setData (short *buf, int vol) {
	short *databuf = data;
	
	for (int i = 0; i < (int)size; i++) {
		databuf[i] = buf[i]*vol/100;
	}
}

