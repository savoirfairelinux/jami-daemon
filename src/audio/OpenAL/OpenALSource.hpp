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

#ifndef __SFLAUDIO_OPENAL_SOURCE_HPP__
#define __SFLAUDIO_OPENAL_SOURCE_HPP__

#include <AL/al.h>
#include <AL/alc.h>
#include "Source.hpp"

namespace SFLAudio
{
  class OpenALContext;

  class OpenALSource : public Source
  {
  private:
    OpenALSource();

  public:
    OpenALSource(int format, int freq, ALuint buffer, ALuint source);
    ~OpenALSource();

    static Source *create(OpenALContext *context, int format, int freq);

    // Source functions
    virtual bool isPlaying();
    virtual void play(void *data, int size);
    virtual void stream(void *data, int size);
    virtual void stop();

  private:
    static bool genBuffer(ALuint &buffer);
    static bool genSource(ALuint &source);
    static bool deleteBuffer(ALuint &buffer);
    static bool deleteSource(ALuint &source);
    static bool attach(ALuint source, ALuint buffer);
    
    
  private:
    // Buffers to hold sound data.
    ALuint mBuffer;

    // Sources are points of emitting sound.
    ALuint mSource;
  };
}

#endif
