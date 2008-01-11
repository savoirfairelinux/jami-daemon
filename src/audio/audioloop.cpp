/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
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
 */
#include "audioloop.h"
#include <math.h>

AudioLoop::AudioLoop() 
{
  _buffer = 0;
  _pos    = 0;
  _size   = 0;
  _sampleRate = 0;
}

AudioLoop::~AudioLoop()
{
  delete [] _buffer; _buffer = 0;
}

int
AudioLoop::getNext(SFLDataFormat* output, int nb, short volume)
{
  int copied = 0;
  int block;
  int pos = _pos;
  while(nb) {
    block = nb;
    if ( block > (_size-pos) ) {
      block = _size-pos;
    }
    // src, dest, len
    bcopy(_buffer+pos, output, block*sizeof(SFLDataFormat)); // short>char conversion
    if (volume!=100) {
      for (int i=0;i<block;i++) {
        *output = (*output * volume)/100;
        output++;
      }
    } else {
      output += block; // this is the destination...
    }
    // should adjust sound here, in output???
    pos = (pos + block ) % _size;
    nb -= block;
    copied += block;
  }
  _pos = pos;
  return copied;
}

