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

struct Info
{
  ALenum format;
  ALvoid *data;
  ALsizei size;
  ALsizei freq;
  ALboolean loop;
};

int main(int argc, char* argv[]) 
{
  AudioLayer *layer = SFLAudio::AudioManager::instance().currentLayer();
  Device *device = layer->openDevice();
  Context *context = device->createContext();

  ALbyte *files[] = {"test.wav", "test2.wav"};


  Info *infos = new Info[argc-1];
  ALenum error;

  for(int i = 0; i < 2; i++) {
    // Load test.wav
    alutLoadWAVFile(files[i],
		    &infos[i].format,
		    &infos[i].data,
		    &infos[i].size,
		    &infos[i].freq,
		    &infos[i].loop);
    error = alGetError();
    if (error != AL_NO_ERROR) {
      std::cerr << "OpenAL: loadWAVFile : " << alGetString(error);
      return 1;
    }

    i++;
  }

  for(int i = 0; i < argc - 1; i++) {
    Source *source = context->createSource(infos[i].format, infos[i].freq);
    source->play(&infos[i].data, infos[i].size);
  }

  for(int i = 0; i < argc - 1; i++) {
    // Unload wav files
    alutUnloadWAV(infos[i].format, 
		  infos[i].data, 
		  infos[i].size, 
		  infos[i].freq);
  }
   
  std::cin.get();
  
  error = alGetError();
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: unloadWAV : " << alGetString(error);
  }
}

