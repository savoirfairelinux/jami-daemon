/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
#ifndef __AUDIOFILE_H__
#define __AUDIOFILE_H__

#include "audio/audioloop.h"
#include "audio/codecs/audiocodec.h"
#include "audio/codecs/codecDescriptor.h"

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
  // Copy Constructor
  AudioFile(const AudioFile& rh);

  // Assignment Operator
  AudioFile& operator=( const AudioFile& rh);

  /** The absolute path to the sound file */
  std::string _filename;
  
  /** Your preferred codec */ 
  AudioCodec* _codec;
  
  /** Start or not */
  bool _start;
};

#endif // __AUDIOFILE_H__
