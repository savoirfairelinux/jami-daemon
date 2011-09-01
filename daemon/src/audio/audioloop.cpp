/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
 *  Inspired by ringbuffer of Audacity Project
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

#include "audioloop.h"
#include <math.h>
#include <cstring>

AudioLoop::AudioLoop() :_buffer (0),  _size (0), _pos (0), _sampleRate (0)
{
}

AudioLoop::~AudioLoop()
{
    delete [] _buffer;
}

void
AudioLoop::getNext (SFLDataFormat* output, int total_samples, short volume)
{
    int pos = _pos;

    if(_size == 0) {
    	_error("AudioLoop: Error: Audio loop size is 0");
    	return;
    }

    while (total_samples) {
        int samples = total_samples;

        if (samples > (_size-pos)) {
            samples = _size-pos;
        }

        memcpy(output, _buffer+pos, samples*sizeof (SFLDataFormat)); // short>char conversion

        if (volume!=100) {
            for (int i=0; i<samples; i++) {
                *output = (*output * volume) /100;
                output++;
            }
        } else {
            output += samples; // this is the destination...
        }

        // should adjust sound here, in output???
        pos = (pos + samples) % _size;

        total_samples -= samples;
    }

    _pos = pos;
}

