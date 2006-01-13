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
{
  mData = (ALchar *)malloc(mSize);
}


void
SFLAudio::MicEmitter::play()
{
  fprintf( stderr, "recording... " );
  mAlCaptureStart();

  ALsizei retval = 0;
  while(retval < mSize) {
    void *data = &mData[retval];
    ALsizei size = mSize - retval;
    retval += mAlCaptureGetData(&mData[retval], 
				mSize - retval,
				getFormat(), 
				getFrequency());
  }
  mAlCaptureStop();

  std::cout << "done." << std::endl;
  std::cout << "playing... ";
  Source *source = getSource();
  if(source && mData) {
    source->play(mData, mSize);
  }
  std::cout << "done." << std::endl;
}
