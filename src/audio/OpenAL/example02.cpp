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
  ALenum format;
  ALvoid *data;
  ALsizei size;
  ALsizei freq;
  ALboolean loop;

  AudioLayer *layer = SFLAudio::AudioManager::instance().currentLayer();
  std::cout << "Layer: " << layer->getName() << std::endl;

  Device *device = layer->openDevice();
  std::cout << "  Device: " << device->getName() << std::endl;
  Context *context = device->createContext();
  std::cout << "  Context is null: " << (context->isNull() ? "true" : "false") << std::endl;

  // Load test.wav
  alutLoadWAVFile("test.wav",&format,&data,&size,&freq,&loop);
  ALenum error = alGetError();
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: loadWAVFile : " << alGetString(error);
    return 1;
  }

  Source *source = context->createSource(format, freq);
  std::cout << "  Source is null: " << (source->isNull() ? "true" : "false") << std::endl;
  source->play(data, size);

  std::cout << "Unloading test.wav" << std::endl;
  // Unload test.wav
  alutUnloadWAV(format, data, size, freq);
  error = alGetError();
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: unloadWAV : " << alGetString(error);
  }


  std::cin.get();
}
