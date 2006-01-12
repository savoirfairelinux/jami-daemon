/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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

#include <iostream>
#include <list>
#include <string>

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alut.h>

#include "SFLAudio.hpp"

using namespace SFLAudio;

int main(int, char* []) 
{
  ALenum format1;
  ALvoid *data1;
  ALsizei size1;
  ALsizei freq1;
  ALboolean loop1;

  ALenum format2;
  ALvoid *data2;
  ALsizei size2;
  ALsizei freq2;
  ALboolean loop2;

  AudioLayer *layer = SFLAudio::AudioManager::instance().currentLayer();
  Device *device = layer->openDevice();
  Context *context = device->createContext();

  // Load test.wav
  alutLoadWAVFile("test.wav",&format1,&data1,&size1,&freq1,&loop1);
  ALenum error = alGetError();
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: loadWAVFile : " << alGetString(error);
    return 1;
  }

  // Load test2.wav
  alutLoadWAVFile("test2.wav",&format2,&data2,&size2,&freq2,&loop2);
  error = alGetError();
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: loadWAVFile : " << alGetString(error);
    return 1;
  }

  Source *source1 = context->createSource(format1, freq1);
  source1->play(data1, size1);
  Source *source2 = context->createSource(format2, freq2);
  source2->play(data2, size2);

  // Unload test.wav and test2.wav
  alutUnloadWAV(format1, data1, size1, freq1);
  alutUnloadWAV(format2, data2, size2, freq2);
  std::cin.get();
  error = alGetError();

  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: unloadWAV : " << alGetString(error);
  }


}

