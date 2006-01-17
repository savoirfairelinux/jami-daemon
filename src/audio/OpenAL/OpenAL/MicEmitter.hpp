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

#ifndef __SFLAUDIO_MIC_EMITTER_HPP__
#define __SFLAUDIO_MIC_EMITTER_HPP__

#include <AL/al.h>
#include <cc++/thread.h>
#include "Emitter.hpp"


namespace SFLAudio
{
  class Source;
  
  class MicEmitterThread : public ost::Thread
  {
  public:
    MicEmitterThread(int format, int freq, int size, PFNALCAPTUREGETDATAPROC palCaptureGetData);
    ~MicEmitterThread();

    void setSource(SFLAudio::Source *source);
    virtual void run();
    void fill();

  private:
    SFLAudio::Source *mSource;
    ALchar *mData;
    ALsizei mFormat;
    ALsizei mFreq;
    ALsizei mSize;

    PFNALCAPTUREGETDATAPROC mAlCaptureGetData;
  };

  class MicEmitter : public Emitter
  {
  private:
    MicEmitter();

  public:
    MicEmitter(int format, int freq, int size,   
	       PFNALCAPTURESTARTPROC palCaptureStart,
	       PFNALCAPTURESTOPPROC palCaptureStop,
	       PFNALCAPTUREGETDATAPROC palCaptureGetData);
    virtual void play();
    virtual void stop();
    

  private:
    ALsizei mSize;
    PFNALCAPTURESTARTPROC mAlCaptureStart;
    PFNALCAPTURESTOPPROC mAlCaptureStop;
    PFNALCAPTUREGETDATAPROC mAlCaptureGetData;
    
    MicEmitterThread* mThread;
  };

}

#endif
