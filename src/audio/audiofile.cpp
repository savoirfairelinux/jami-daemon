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
#include "audiofile.h"
#include "codecDescriptor.h"
#include <fstream>

AudioFile::AudioFile()
 : AudioLoop()
{
  // could vary later...
  _ulaw = new Ulaw (PAYLOAD_CODEC_ULAW, "G711u");
  _start = false;
}


AudioFile::~AudioFile()
{
  delete _ulaw;
}

bool
AudioFile::loadFile(const std::string& filename) 
{
  if (_filename == filename) {
    return true;
  } else {
    // reset to 0
    delete [] _buffer; _buffer = 0;
    _size = 0;
    _pos = 0;
  }

  // no filename to load
  if (filename.empty()) {
    return false;
  }

  std::fstream file;
  file.open(filename.c_str(), std::fstream::in);
  if (!file.is_open()) {
    // unable to load the file
    return false;
  }

  // get length of file:
  file.seekg (0, std::ios::end);
  int length = file.tellg();
  file.seekg (0, std::ios::beg);

  // allocate memory:
  char fileBuffer[length];
  // read data as a block:
  file.read (fileBuffer,length);
  file.close();

  // Decode file.ul
  // expandedsize is the number of bytes, not the number of int
  // expandedsize should be exactly two time more, else failed
  int16 monoBuffer[length];
  unsigned int expandedsize = _ulaw->codecDecode (monoBuffer, (unsigned char *) fileBuffer, length);

  if (expandedsize != length*sizeof(int16)) {
    _debug("Audio file error on loading audio file!");
    return false;
  }

  unsigned int int16size = expandedsize/sizeof(int16);
  _size = int16size<<1; // multiply by two
  _buffer = new int16[_size];

  for(unsigned int i=0, k=0; i<int16size; i++) {
    _buffer[k] = _buffer[k+1] = monoBuffer[i];
    k+=2;
  }

  return true;
}

