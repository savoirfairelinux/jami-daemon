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
#include <AL/al.h>
#include <AL/alc.h>


#include "OpenALDevice.hpp"
#include "OpenALContext.hpp"
#include "OpenALLayer.hpp"
#include "NullContext.hpp"
#include "NullDevice.hpp"

SFLAudio::OpenALDevice::OpenALDevice()
  : mDevice(0)
{}

SFLAudio::OpenALDevice::~OpenALDevice()
{
  unload();
}

void
SFLAudio::OpenALDevice::unload() {
  if(mDevice) {
    if(alcCloseDevice(mDevice) == ALC_FALSE) {
      ALenum error = alcGetError(mDevice);
      std::cerr << "OpenAL::alcCloseDevice: " << alGetString(error) << std::endl;
    }
    mDevice = 0;
  }
}


bool
SFLAudio::OpenALDevice::load() {
  mDevice = alcOpenDevice(0);
  ALenum error = alcGetError(mDevice);
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL::alcOpenDevice: " << alGetString(error) << std::endl;
    unload();
  }

  if(mDevice != 0) {
    const ALCchar *device = alcGetString(mDevice, ALC_DEVICE_SPECIFIER);
    setName(device);
  }

  return mDevice;
}


SFLAudio::Context *
SFLAudio::OpenALDevice::createContext()
{
  SFLAudio::Context *context = NULL;
  if (mDevice) {
    ALCcontext *c = alcCreateContext(mDevice, NULL);
    alcMakeContextCurrent(c);
    if(c == NULL) {
      ALenum error = alcGetError(mDevice);
      if (error != AL_NO_ERROR) {
	std::cerr << "OpenAL::alcCreateContext: " << alGetString(error) << std::endl;
      }
      context = new NullContext();
    }
    else {
      alcMakeContextCurrent(c);
      context = new OpenALContext(c);
    }
  }
  else {
    context = new NullContext();
  }

  return context;
}
