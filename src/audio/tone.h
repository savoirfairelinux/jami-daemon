/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of 
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __TONE_H__
#define __TONE_H__

#include <string>
#include "../global.h" // for int16 declaration and SAMPLING_RATE
#define TONE_NBTONE 4
#define TONE_NBCOUNTRY 7

/**
 * @author Yan Morin <yan.morin@savoirfairelinux.com>
 */
class Tone {
public:
  Tone(const std::string& definition);
  ~Tone();
  enum TONEID {
    TONE_DIALTONE = 0,
    TONE_BUSY,
    TONE_RINGTONE,
    TONE_CONGESTION,
    TONE_NULL
  };

  /**
   * get the next fragment of the tone
   * the function change the intern position, and will loop
   * @param nb of int16 (mono) to send
   * @return the number of int16 sent (nb*2)
   */
  int getNext(int16* output, int nb, short volume=100);
  void reset() { _pos = 0; }

private:
  /**
   * add a simple or double sin to the buffer, it double the sin in stereo 
   * @param nb are the number of int16 (mono) to generate
   * by example nb=5 generate 10 int16, 5 for the left, 5 for the right
   */
  void genSin(int16 *buffer, int frequency1, int frequency2, int nb);

  /**
   * allocate the memory with the definition
   */
  void genBuffer(const std::string& definition);

  int16* _buffer;
  int _size; // number of int16 inside the buffer, not the delay
  int _pos; // current position, set to 0, when initialize
};

#endif // __TONE_H__

