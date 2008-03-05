/*
 *  Copyright (C) 2005-2007 Savoir-Faire Linux inc.
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
#ifndef __AUDIOFILE_H__
#define __AUDIOFILE_H__

#include "audioloop.h"
#include "codecs/audiocodec.h"
#include "codecDescriptor.h"

/**
	@author Yan Morin <yan.morin@savoirfairelinux.com>
*/
class AudioFile : public AudioLoop
{
public:
  AudioFile();
  ~AudioFile();

  bool loadFile(const std::string& filename, AudioCodec *codec , unsigned int sampleRate/*=8000*/);
  void start() { _start = true; }
  void stop()  { _start = false; }
  bool isStarted() { return _start; }

private:
  std::string _filename;
  AudioCodec* _codec;
  bool _start;
};

#endif // __AUDIOFILE_H__
