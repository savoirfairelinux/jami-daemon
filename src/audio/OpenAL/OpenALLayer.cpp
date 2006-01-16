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
#include "MicEmitter.hpp"
#include "NullDevice.hpp"
#include "NullEmitter.hpp"

#include <iostream>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/alut.h>

#define DEFAULT_DEVICE_NAME "default"
#define DEFAULT_CAPTURE_DEVICE_NAME "default"

#define MIC_FORMAT AL_FORMAT_MONO8
#define FREQ          8192
#define SAMPLES       (5 * FREQ)
#define SIZE (SAMPLES * 1)

#define GP(type,var,name) \
        var = (type)alGetProcAddress((const ALchar*) name); \
	if( var == NULL ) { \
		fprintf( stderr, "Could not get %s extension entry\n", name ); \
	}



SFLAudio::OpenALLayer::OpenALLayer() 
  : AudioLayer("openal")
{
  alutInit(0, 0);
  GP( PFNALCAPTUREINITPROC, palCaptureInit, "alCaptureInit_EXT" );
  GP( PFNALCAPTUREDESTROYPROC, palCaptureDestroy,
      "alCaptureDestroy_EXT" );
  GP( PFNALCAPTURESTARTPROC, palCaptureStart, "alCaptureStart_EXT" );
  GP( PFNALCAPTURESTOPPROC, palCaptureStop, "alCaptureStop_EXT" );
  GP( PFNALCAPTUREGETDATAPROC, palCaptureGetData,
      "alCaptureGetData_EXT" );
}


SFLAudio::Emitter *
SFLAudio::OpenALLayer::openCaptureDevice()
{
  if(palCaptureInit == 0 || 
     !palCaptureInit( MIC_FORMAT, FREQ, 1024 ) ) {
    printf( "Unable to initialize capture\n" );
    return new NullEmitter();
  }

  return new MicEmitter(MIC_FORMAT, FREQ, SIZE,
			palCaptureStart,
			palCaptureStop,
			palCaptureGetData);
}


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
  else {
    devices.push_back(DEFAULT_DEVICE_NAME);
  }
  

  return devices;

}

std::list< std::string > 
SFLAudio::OpenALLayer::getCaptureDevicesNames() 
{
  std::list< std::string > devices;
  if (alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_TRUE) {
    const ALCchar *devs = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
    const ALCchar *devname = devs;
    while(devname) {
      devices.push_back(devname);
      devname += sizeof(ALCchar) * (strlen(devname) + 1);
    }
  }
  else {
    devices.push_back(DEFAULT_CAPTURE_DEVICE_NAME);
  }
  

  return devices;

}


SFLAudio::Device *
SFLAudio::OpenALLayer::openDevice()
{
  Device *dev = new OpenALDevice();
  if(dev->load()) {
    dev->setName(DEFAULT_DEVICE_NAME);
  }
  else {
    delete dev;
    dev = new NullDevice();
  }

  return dev;
}

SFLAudio::Device *
SFLAudio::OpenALLayer::openDevice(const std::string &)
{
  return openDevice();
}


