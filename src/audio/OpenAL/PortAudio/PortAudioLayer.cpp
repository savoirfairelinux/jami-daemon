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

#include "PortAudio/PortAudioLayer.hpp"
#include "portaudio.h"

#include "Null/NullDevice.hpp"
#include "Null/NullEmitter.hpp"


SFLAudio::PortAudioLayer::PortAudioLayer()
  : AudioLayer("portaudio")
{
  Pa_Initialize();
}

SFLAudio::PortAudioLayer::~PortAudioLayer()
{
  Pa_Terminate();
}



std::list< std::string >
SFLAudio::PortAudioLayer::getDevicesNames() 
{
  refreshDevices();

  std::list< std::string > devices;
  for(DevicesType::iterator pos = mDevices.begin();
      pos != mDevices.end();
      pos++) {
    devices.push_back(pos->first);
  }

  return devices;
}


std::list< std::string >
SFLAudio::PortAudioLayer::getCaptureDevicesNames() 
{
  return std::list< std::string >();
}


void
SFLAudio::PortAudioLayer::refreshDevices() 
{
  mDevices.clear();
  for(int index = 0; index < Pa_GetDeviceCount(); index++ ) {
    const PaDeviceInfo *device = NULL;
    const PaHostApiInfo *host = NULL;

    device = Pa_GetDeviceInfo(index);
    if(device != NULL) {
      host = Pa_GetHostApiInfo(device->hostApi);
    }
    
    if(device != NULL && host != NULL) {
      std::string name(host->name);
      name += ": ";
      name += device->name;
      mDevices.insert(std::make_pair(name, index));
    }
  }
}

SFLAudio::Device *
SFLAudio::PortAudioLayer::openDevice()
{
  return new NullDevice();
}

SFLAudio::Emitter *
SFLAudio::PortAudioLayer::openCaptureDevice()
{
  return new NullEmitter();
}

SFLAudio::Device *
SFLAudio::PortAudioLayer::openDevice(const std::string &)
{
  return new NullDevice();
}
