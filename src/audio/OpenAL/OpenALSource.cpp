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

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alut.h>
#include <iostream>
#include <unistd.h>

#include "Context.hpp"
#include "NullSource.hpp"
#include "OpenALSource.hpp"

SFLAudio::OpenALSource::OpenALSource(int format, int freq, ALuint buffer, ALuint source)
  : Source(format, freq)
  , mBuffer(buffer)
  , mSource(source)
  , mIsAttached(false)
  , mIsStatic(true)
{
  mBuffers.push_back(buffer);
}

SFLAudio::OpenALSource::~OpenALSource()
{
  alGetError();
  
  alDeleteSources(1, &mSource);
  ALenum error = alGetError();
  if(error != AL_NO_ERROR) {
    std::cerr << "OpenAL: alDeleteSources : " << alGetString(error) << std::endl;
  }

  for(std::list< ALuint >::iterator pos = mBuffers.begin();
      pos != mBuffers.end();
      pos++) {
    alDeleteBuffers(1, &(*pos));
    error = alGetError();
    if(error != AL_NO_ERROR) {
      std::cerr << "OpenAL: alDeleteBuffers : " << alGetString(error) << std::endl;
    }
  }
}

 
bool
SFLAudio::OpenALSource::genBuffer(ALuint &buffer) {
  // Generate buffer
  alGetError(); // clear error code
  alGenBuffers(1, &buffer);
  ALenum error = alGetError();
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: alGenBuffers : " << alGetString(error) << std::endl;;
    return false;
  }
  
  return true;
}

bool
SFLAudio::OpenALSource::genSource(ALuint &source) {
  // Generate buffer
  alGetError(); // clear error code
  alGenSources(1, &source);
  ALenum error = alGetError();
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: alGenSources : " << alGetString(error) << std::endl;;
    return false;
  }

  alSource3f(source, AL_POSITION, 0.0, 0.0, 0.0);
  alSource3f(source, AL_VELOCITY, 0.0, 0.0, 0.0);
  alSource3f(source, AL_DIRECTION, 0.0, 0.0, 0.0);
  alSourcef (source, AL_ROLLOFF_FACTOR, 0.0);
  alSourcei (source, AL_SOURCE_RELATIVE, AL_TRUE);
  
  return true;
}

bool
SFLAudio::OpenALSource::deleteSource(ALuint &source) {
  // Generate buffer
  alGetError(); // clear error code
  alDeleteSources(1, &source);
  ALenum error = alGetError();
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: alDeleteSources : " << alGetString(error) << std::endl;;
    return false;
  }
  
  return true;
}

bool
SFLAudio::OpenALSource::deleteBuffer(ALuint &buffer) {
  // Generate buffer
  alGetError(); // clear error code
  alDeleteBuffers(1, &buffer);
  ALenum error = alGetError();
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: alDeleteBuffers : " << alGetString(error) << std::endl;;
    return false;
  }
  
  return true;
}

bool
SFLAudio::OpenALSource::detach() 
{
  alSourcei(mSource, AL_BUFFER, 0);
  ALenum error = alGetError();
  if(error != AL_NO_ERROR) {
    std::cerr << "OpenAL: alSourcei : " << alGetString(error);
    return false;
  }

  mIsAttached = false;

  return true;
}

ALuint
SFLAudio::OpenALSource::getSourceState()
{
//   ALint answer;
//   alGetError();
  
//   alGetSourcei(mSource, AL_SOURCE_TYPE, &answer);
//   ALenum error = alGetError();
//   if(error != AL_NO_ERROR) {
//     std::cerr << "OpenAL: alGetSourcei(state) : " << alGetString(error);
//     return AL_UNDETERMINED;
//   }
  
//   return answer;

  if(!mIsAttached) {
    return AL_UNDETERMINED;
  }

  if(mIsStatic) {
    return AL_STATIC;
  }

  return AL_STREAMING;
}

bool 
SFLAudio::OpenALSource::isStatic()
{
  return (getSourceState() == AL_STATIC);
}

bool 
SFLAudio::OpenALSource::isUndetermined()
{
  return (getSourceState() == AL_UNDETERMINED);
}

bool 
SFLAudio::OpenALSource::isStreaming()
{
  return (getSourceState() == AL_STREAMING);
}

bool 
SFLAudio::OpenALSource::isAttached()
{
  return (getSourceState() != AL_UNDETERMINED);
}

bool check(const char *message) {
  ALenum error = alGetError();
  if(error != AL_NO_ERROR) {
    std::cerr << message << alGetString(error);
    return false;
  }

  return true;
}

bool
SFLAudio::OpenALSource::attach() 
{
  if(isAttached()) {
    if(isStatic() == true) {
      // it's already attached as static
      return true;
    }
    
    detach();
  }

  // Attach buffer to source
  alSourcei(mSource, AL_BUFFER, mBuffer);
  ALenum error = alGetError();
  if(error != AL_NO_ERROR) {
    std::cerr << "OpenAL: alSourcei : " << alGetString(error);
    return false;
  }

  mIsAttached = true;
  mIsStatic = true;

  return true;
}

SFLAudio::Source *
SFLAudio::OpenALSource::create(OpenALContext *, int format, int freq) {
  ALuint buffer;
  ALuint source;
  
  // Generate buffer
  if(!genBuffer(buffer)){
    deleteBuffer(buffer);
    return new NullSource();
  }
  
  // Generate source
  if(!genSource(source)){
    deleteBuffer(buffer);
    deleteSource(source);
    return new NullSource();
  }
  
  return new OpenALSource(format, freq, buffer, source);
}


bool
SFLAudio::OpenALSource::isPlaying() 
{
  ALint state;
  if(alIsSource(mSource) == AL_FALSE) {
    return false;
  }

  alGetSourcei(mSource, AL_SOURCE_STATE, &state);
    
  return (state == AL_PLAYING);
}

void
SFLAudio::OpenALSource::stream(void *data, int size)
{
  int processed;
  ALuint buffer;

  alGetSourcei(mSource, AL_BUFFERS_PROCESSED, &processed);
  
  if(processed == 0) {
    if(genBuffer(buffer) == false) {
      return;
    }

    // Attach streaming buffer to source
    alSourceQueueBuffers(mSource, 1, &buffer);
    ALenum error = alGetError();
    if(error != AL_NO_ERROR) {
      std::cerr << "OpenAL: alSourceQueueBuffers : " << alGetString(error);
      return;
    }
    mBuffers.push_back(buffer);

    mIsAttached = true;
    mIsStatic = false;

  }
  else {
    alSourceUnqueueBuffers(mSource, 1, &buffer);
  }
    
  alBufferData(buffer, getFormat(), data, size, getFrequency());
  alSourceQueueBuffers(mSource, 1, &buffer);

  if(!isPlaying()) {
    alSourcePlay(mSource);
  }
}

void
SFLAudio::OpenALSource::play(void *data, int size)
{
  attach();

  alGetError();

  // Copy data into AL Buffer 
  alBufferData(mBuffer, getFormat(), data, size, getFrequency());
  ALenum error = alGetError();
  if (error != AL_NO_ERROR) {
    std::cerr << "OpenAL: alBufferData : " << alGetString(error);
  }

  alSourcePlay(mSource);
}

void
SFLAudio::OpenALSource::stop()
{
  alSourceStop(mSource);
}
