/*
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of 
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
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
 */
#ifndef __AUDIOLOOP_H__
#define __AUDIOLOOP_H__

#include <string>
#include "../global.h" // for int16 declaration

/**
 * @file audioloop.h
 * @brief Loop on a sound file
 */

class AudioLoop {
public:
  /**
   * Constructor
   */
  AudioLoop();
  
  /**
   * Virtual destructor
   */
  virtual ~AudioLoop();

  /**
   * Get the next fragment of the tone
   * the function change the intern position, and will loop
   * @param output  The data buffer
   * @param nb of int16 to send
   * @param volume  The volume
   * @return the number of int16 sent (nb*2)
   */
  int getNext(SFLDataFormat* output, int nb, short volume=100);
  
  /**
   * Reset the pointer position
   */ 
  void reset() { _pos = 0; }

  /**
   * Accessor to the size of the buffer
   * @return unsigned int The size
   */
  unsigned int getSize() { return _size; }
  


protected:
  /** The data buffer */
  SFLDataFormat* _buffer;

  /** Number of int16 inside the buffer, not the delay */
  int _size;  

  /** current position, set to 0, when initialize */
  int _pos;  

  /** Sample rate */
  int _sampleRate; 
};

#endif // __AUDIOLOOP_H__

