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
#include "tone.h"
#include <math.h>

int INT16_AMPLITUDE = 32767;

Tone::Tone(const std::string& definition, unsigned int sampleRate, unsigned int nbChannel) : AudioLoop()
{
  _nbChannel  = nbChannel;
  _sampleRate = sampleRate;
  genBuffer(definition); // allocate memory with definition parameter
}

Tone::~Tone()
{
}

void 
Tone::genBuffer(const std::string& definition)
{
  if (definition.empty()) { return; }
  _size = 0;

  int16 *buffer = new int16[SIZEBUF]; //1kb
  int16 *bufferPos = buffer;

  // Number of format sections 
  unsigned int posStart = 0; // position of precedent comma
  unsigned int posEnd = 0; // position of the next comma

  std::string s; // portion of frequency
  int count; // number of int for one sequence

  unsigned int deflen = definition.length();
  do {
    posEnd = definition.find(',', posStart);
    if (posEnd == std::string::npos) {
      posEnd = deflen;
    }

    {
      // Sample string: "350+440" or "350+440/2000,244+655/2000"
      int freq1, freq2, time;
      s = definition.substr(posStart, posEnd-posStart);

      // The 1st frequency is before the first + or the /
      unsigned int pos_plus = s.find('+');
      unsigned int pos_slash = s.find('/');
      unsigned int len = s.length();
      unsigned int endfrequency = 0;

      if ( pos_slash == std::string::npos ) {
        time = 0;
        endfrequency = len;
      } else {
        time = atoi((s.substr(pos_slash+1,len-pos_slash-1)).data());
        endfrequency = pos_slash;
      }

      // without a plus = 1 frequency
      if (pos_plus == std::string::npos ) {
        freq1 = atoi((s.substr(0,endfrequency)).data());
        freq2 = 0;
      } else {
        freq1 = atoi((s.substr(0,pos_plus)).data());
        freq2 = atoi((s.substr(pos_plus+1, endfrequency-pos_plus-1)).data());
      }

      // If there is time or if it's unlimited
      if (time == 0) {
        count = _sampleRate;
      } else {
        count = (_sampleRate * time) / 1000;
      }
      // Generate SAMPLING_RATE samples of sinus, buffer is the result
      genSin(bufferPos, freq1, freq2, count);

      // To concatenate the different buffers for each section.
      _size += (count * _nbChannel); 
      bufferPos += (count * _nbChannel);
    }

    posStart = posEnd+1;
  } while (posStart < deflen);

  _buffer = new int16[_size];
  // src, dest, tocopy
  bcopy(buffer, _buffer, _size<<1); // copy char, not int16..
  delete[] buffer; buffer=0; bufferPos=0;
}

void
Tone::genSin(int16 *buffer, int frequency1, int frequency2, int nb) 
{
  double var1 = (double)2 * (double)M_PI * (double)frequency1 / (double)_sampleRate; 
  double var2 = (double)2 * (double)M_PI * (double)frequency2 / (double)_sampleRate;

  // softer
  double amp = (double)(INT16_AMPLITUDE >> 4);
  if (_nbChannel == 2) { // stereo
    int k = 0;
    for(int t = 0; t < nb; t++) {
      k = t << 1; // double channel : left/right
      buffer[k] = buffer[k+1] = (int16)(amp * ((sin(var1 * t) + sin(var2 * t))));
    }
  } else {
    for(int t = 0; t < nb; t++) {
      buffer[t] = (int16)(amp * ((sin(var1 * t) + sin(var2 * t))));
    }
  }
}

