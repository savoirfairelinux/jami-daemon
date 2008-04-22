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
 * @file audiofile.h
 * @brief A class to manage sound files
 */

class AudioFile : public AudioLoop
{
public:
  /**
   * Constructor
   */
  AudioFile();
  
  /**
   * Destructor
   */
  ~AudioFile();

  /**
   * Load a sound file in memory
   * @param filename  The absolute path to the file
   * @param codec     The codec to decode and encode it
   * @param sampleRate	The sample rate to read it
   * @return bool   True on success
   */
  bool loadFile(const std::string& filename, AudioCodec *codec , unsigned int sampleRate);
  
  /**
   * Start the sound file
   */
  void start() { _start = true; }
  
  /**
   * Stop the sound file
   */
  void stop()  { _start = false; }
  
  /**
   * Tells whether or not the file is playing
   * @return bool True if yes
   *		  false otherwise
   */
  bool isStarted() { return _start; }

private:
  /** The absolute path to the sound file */
  std::string _filename;
  
  /** Your preferred codec */ 
  AudioCodec* _codec;
  
  /** Start or not */
  bool _start;
};

#endif // __AUDIOFILE_H__
