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

#include <AL/alut.h>
#include <AL/alut.h>
#include <iostream>

#include "OpenALLayer.hpp"
#include "MicEmitter.hpp"
#include "Source.hpp"

SFLAudio::MicEmitter::MicEmitter(int format, int freq, int size,
				 PFNALCAPTURESTARTPROC palCaptureStart,
				 PFNALCAPTURESTOPPROC palCaptureStop,
				 PFNALCAPTUREGETDATAPROC palCaptureGetData)
  : Emitter(format, freq)
    , mSize(size)
    , mAlCaptureStart(palCaptureStart)
    , mAlCaptureStop(palCaptureStop)
    , mAlCaptureGetData(palCaptureGetData)
    , mThread(0)
{}

void
SFLAudio::MicEmitter::play()
{
  if(mThread == 0) {
    mAlCaptureStart();
    
    mThread = new MicEmitterThread(getFormat(), getFrequency(), mSize, mAlCaptureGetData);
    mThread->setSource(getSource());
    mThread->start();
  }
}

void
SFLAudio::MicEmitter::stop()
{
  if(mThread != 0) {
    delete mThread;
    mThread = 0;
  }
}


SFLAudio::MicEmitterThread::MicEmitterThread(int format,
					     int freq,
					     int size,
					     PFNALCAPTUREGETDATAPROC palCaptureGetData)
  : mSource(0)
    , mFormat(format)
    , mFreq(freq)
  , mSize(size)
  , mAlCaptureGetData(palCaptureGetData)
{
  setCancel(cancelDeferred);
  mData = (ALchar *)malloc(mSize);
}

SFLAudio::MicEmitterThread::~MicEmitterThread()
{
  terminate();
  free(mData);
}

void
SFLAudio::MicEmitterThread::setSource(SFLAudio::Source *source) {
  mSource = source;
}

void
SFLAudio::MicEmitterThread::fill() {
  ALsizei retval = 0;
  std::cout << "filling capture buffer...\n";
  while(retval < mSize) {
    int size = mAlCaptureGetData(&mData[retval], 
				 mSize - retval,
				 mFormat, 
				 mFreq);
    retval += size;
    if(size != 0)
      std::cout << "read " << size << 
	" bytes from capture, for a total of " << retval << std::endl;
  }
  std::cout << "capture buffer filled!\n";
}

void
SFLAudio::MicEmitterThread::run()
{
  while (!testCancel()) {
    if(mData) {
      fill();
    }

    if(mSource && mData) {
      mSource->stream(mData, mSize);
    }
    else {
      std::cerr << "source or buffer invalid.\n";
    }
    std::cout << "done." << std::endl;
  }
}
