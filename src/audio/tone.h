/*
 *  Copyright (C) 2005-2006 Savoir-Faire Linux inc.
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
#ifndef __TONE_H__
#define __TONE_H__

#include <string>
#include "audioloop.h"

#define TONE_NBTONE 4
#define TONE_NBCOUNTRY 7

/**
 * Tone sample (dial, busy, ring, congestion)
 * @author Yan Morin <yan.morin@savoirfairelinux.com>
 */
class Tone : public AudioLoop {
public:
  /**
   * @param definition String that contain frequency/time of the tone
   * @param sampleRate SampleRating of audio tone
   */
  Tone(const std::string& definition, unsigned int sampleRate);
  ~Tone();
  enum TONEID {
    TONE_DIALTONE = 0,
    TONE_BUSY,
    TONE_RINGTONE,
    TONE_CONGESTION,
    TONE_NULL
  };
  /**
   * add a simple or double sin to the buffer, it double the sin in stereo 
   * @param nb are the number of int16 (mono) to generate
   * by example nb=5 generate 10 int16, 5 for the left, 5 for the right
   */
  void genSin(SFLDataFormat* buffer, int frequency1, int frequency2, int nb);

private:

  /**
   * allocate the memory with the definition
   */
  void genBuffer(const std::string& definition);

  unsigned int _sampleRate;
};

#endif // __TONE_H__

