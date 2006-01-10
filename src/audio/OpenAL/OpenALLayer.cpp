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

#include "OpenALLayer.hpp"
#include "OpenALDevice.hpp"
#include "NullDevice.hpp"

#include <iostream>
#include <AL/al.h>
#include <AL/alc.h>

SFLAudio::OpenALLayer::OpenALLayer() 
  : AudioLayer("openal")
{}

std::list< std::string > 
SFLAudio::OpenALLayer::getDevicesNames() 
{
  std::list< std::string > devices;
  if (alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_TRUE) {
    const ALCchar *devs = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    const ALCchar *devname = devs;
    while(devname) {
      devices.push_back(devname);
      devname += sizeof(ALCchar) * (strlen(devname) + 1);
    }
  }

  return devices;

}

SFLAudio::Device *
SFLAudio::OpenALLayer::openDevice()
{
  Device *dev = new OpenALDevice();
  if(dev->load() == false) {
    delete dev;
    dev = new NullDevice();
  }

  return dev;
}

SFLAudio::Device *
SFLAudio::OpenALLayer::openDevice(const std::string &)
{
  return new NullDevice();
}

void
SFLAudio::OpenALLayer::assertError()
{
  ALenum error;
  if ((error = alGetError()) != AL_NO_ERROR) {
    std::cerr << "OpenAL::alcOpenDevice: " << alGetString(error) << std::endl;
  }
}

void
SFLAudio::OpenALLayer::clearError()
{
  alGetError();
}
