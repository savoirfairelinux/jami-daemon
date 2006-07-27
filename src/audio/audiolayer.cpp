/*
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Jerome Oufella <jerome.oufella@savoirfairelinux.com> 
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

#include <stdio.h>
#include <stdlib.h>

#include "audiolayer.h"
#include "../global.h"
#include "../manager.h"

AudioLayer::AudioLayer()
  : _urgentRingBuffer(SIZEBUF)
  , _mainSndRingBuffer(SIZEBUF)
  , _micRingBuffer(SIZEBUF)
  , _stream(NULL)
  , _errorMessage("")
{
  _sampleRate = 8000;
  
  _inChannel  = 1;
  _outChannel = 1;
  portaudio::System::initialize();
}

// Destructor
AudioLayer::~AudioLayer (void) 
{
  try {
    portaudio::System::terminate();
  } catch (const portaudio::PaException &e) {
    _debug("Catch an exception when portaudio tried to terminate\n");
  }
  closeStream();
}

void
AudioLayer::closeStream (void) 
{
  ost::MutexLock guard(_mutex);
  if(_stream) {
    _stream->close();
    delete _stream; _stream = NULL;
  }
}

bool
AudioLayer::hasStream(void) {
  ost::MutexLock guard(_mutex);
  return (_stream!=0 ? true : false); 
}


void
AudioLayer::openDevice (int indexIn, int indexOut, int sampleRate) 
{
  closeStream();

  try {
  // Set up the parameters required to open a (Callback)Stream:
  portaudio::DirectionSpecificStreamParameters 
    outParams(portaudio::System::instance().deviceByIndex(indexOut), 
	      _outChannel, portaudio::INT16, true, 
	      portaudio::System::instance().deviceByIndex(indexOut).defaultLowOutputLatency(), 
	      NULL);
	
  portaudio::DirectionSpecificStreamParameters 
    inParams(portaudio::System::instance().deviceByIndex(indexIn), 
	     _inChannel, portaudio::INT16, true, 
	     portaudio::System::instance().deviceByIndex(indexIn).defaultLowInputLatency(), 
	     NULL);
	
  #ifdef USE_SAMPLERATE
  _sampleRate = sampleRate;
  #else
  _sampleRate = 8000;
  #endif
  
  // we could put paFramesPerBufferUnspecified instead of FRAME_PER_BUFFER to be variable
  portaudio::StreamParameters const params(inParams, outParams, 
					   _sampleRate, FRAME_PER_BUFFER /*paFramesPerBufferUnspecified*/, paNoFlag /*paPrimeOutputBuffersUsingStreamCallback | paNeverDropInput*/);
		  
  // Create (and open) a new Stream, using the AudioLayer::audioCallback
  ost::MutexLock guard(_mutex);
  _stream = new portaudio::MemFunCallbackStream<AudioLayer>(params, 
							    *this, 
							    &AudioLayer::audioCallback);
  } catch(...) {
    throw;
  }
}

void
AudioLayer::startStream(void) 
{
  try {
    ost::MutexLock guard(_mutex);
    if (_stream && !_stream->isActive()) {
      //_debug("Starting sound stream\n");
        _stream->start();
    }
  } catch (const portaudio::PaException &e) {
    _debugException("Portaudio error: error on starting audiolayer stream");
    throw;
  } catch(...) {
    _debugException("stream start error");
    throw;
  }
}
	
void
AudioLayer::stopStream(void) 
{
  try {
    ost::MutexLock guard(_mutex);
    if (_stream && !_stream->isStopped()) {
      _stream->stop();
      _mainSndRingBuffer.flush();
      _urgentRingBuffer.flush();
      _micRingBuffer.flush();
    }
  } catch (const portaudio::PaException &e) {
    _debugException("Portaudio error: error on stoping audiolayer stream");
    throw;
  } catch(...) {
    _debugException("stream stop error");
    throw;
  }
}

void
AudioLayer::sleep(int msec) 
{
  if (_stream) {
    portaudio::System::instance().sleep(msec);
  }
}

bool
AudioLayer::isStreamActive (void) 
{
  ost::MutexLock guard(_mutex);
  if(_stream && _stream->isActive()) {
    return true;
  }
  else {
    return false;
  }
}

int 
AudioLayer::putMain(void* buffer, int toCopy)
{
  ost::MutexLock guard(_mutex);
  if (_stream) {
    int a = _mainSndRingBuffer.AvailForPut();
    if ( a >= toCopy ) {
      return _mainSndRingBuffer.Put(buffer, toCopy);
    } else {
      return _mainSndRingBuffer.Put(buffer, a);
    }
  }
  return 0;
}

void
AudioLayer::flushMain()
{
  ost::MutexLock guard(_mutex);
  _mainSndRingBuffer.flush();
}

int
AudioLayer::putUrgent(void* buffer, int toCopy)
{
  if (_stream) {
    int a = _urgentRingBuffer.AvailForPut();
    if ( a >= toCopy ) {
      return _urgentRingBuffer.Put(buffer, toCopy);
    } else {
      return _urgentRingBuffer.Put(buffer, a);
    }
  }
  return 0;
}

int
AudioLayer::canGetMic()
{
  if (_stream) {
    return _micRingBuffer.AvailForGet();
  } else {
    return 0;
  }
}

int 
AudioLayer::getMic(void *buffer, int toCopy)
{
  if(_stream) {
    return _micRingBuffer.Get(buffer, toCopy, 100);
  } else {
    return 0;
  }
}

void
AudioLayer::flushMic()
{
  _micRingBuffer.flush();
}

bool
AudioLayer::isStreamStopped (void) 
{
  ost::MutexLock guard(_mutex);
  if(_stream && _stream->isStopped()) {
    return true;
  }
  else {
    return false;
  }
}

int 
AudioLayer::audioCallback (const void *inputBuffer, void *outputBuffer, 
			   unsigned long framesPerBuffer, 
			   const PaStreamCallbackTimeInfo *timeInfo, 
			   PaStreamCallbackFlags statusFlags) {

  (void) timeInfo;
  (void) statusFlags;
	
  int16 *in  = (int16 *) inputBuffer;
  int16 *out = (int16 *) outputBuffer;
  int toGet; 
  int toPut;
  int urgentAvail; // number of int16 right and int16 left
  int normalAvail; // number of int16 right and int16 left
  int micAvailPut;
  unsigned short spkrVolume = Manager::instance().getSpkrVolume();
  unsigned short micVolume  = Manager::instance().getMicVolume();

  // AvailForGet tell the number of chars inside the buffer
  // framePerBuffer are the number of int16 for one channel (left)
  urgentAvail = _urgentRingBuffer.AvailForGet();
  if (urgentAvail > 0) {
  // Urgent data (dtmf, incoming call signal) come first.		
    toGet = (urgentAvail < (int)(framesPerBuffer * sizeof(int16) * _outChannel)) ? urgentAvail : framesPerBuffer * sizeof(int16) * _outChannel;
    _urgentRingBuffer.Get(out, toGet, spkrVolume);
    
    // Consume the regular one as well (same amount of bytes)
    _mainSndRingBuffer.Discard(toGet);
  }	else {
    AudioLoop* tone = Manager::instance().getTelephoneTone();
    if ( tone != 0) {
      tone->getNext(out, framesPerBuffer*_outChannel, spkrVolume);
    } else if ( (tone=Manager::instance().getTelephoneFile()) != 0 ) {
      tone->getNext(out, framesPerBuffer*_outChannel, spkrVolume);
    } else {
      // If nothing urgent, play the regular sound samples
      normalAvail = _mainSndRingBuffer.AvailForGet();
      toGet = (normalAvail < (int)(framesPerBuffer * sizeof(int16) * _outChannel)) ? normalAvail : framesPerBuffer * sizeof(int16) * (int)_outChannel;

      if (toGet) {
        _mainSndRingBuffer.Get(out, toGet, spkrVolume);
      } else {
	//_debug("padding %d...\n", (int)(framesPerBuffer * sizeof(int16)*_outChannel));
	//_mainSndRingBuffer.debug();
        bzero(out, framesPerBuffer * sizeof(int16) * _outChannel);
      }
    }
  }

  // Additionally handle the mic's audio stream 
  micAvailPut = _micRingBuffer.AvailForPut();
  toPut = (micAvailPut <= (int)(framesPerBuffer * sizeof(int16) * _inChannel)) ? micAvailPut : framesPerBuffer * sizeof(int16) * _inChannel;
  _micRingBuffer.Put(in, toPut, micVolume );

  return paContinue;
}

unsigned int
AudioLayer::convert(int16* fromBuffer, int fromChannel, unsigned int fromSize, int16** toBuffer, int toChannel) {
  if (fromChannel == toChannel ) { // 1=>1 or 2=>2
    // src, dest, size in bytes
    *toBuffer = fromBuffer;
    //bcopy(fromBuffer, *toBuffer, fromSize*sizeof(int16));
    return fromSize;
  } else if (fromChannel > toChannel ) { // 2 => 1
    unsigned int toSize = fromSize>>1; // divise by 2
    for(unsigned int m=0, s=0; m<toSize; m++) {
      s = m<<1;
      (*toBuffer)[m] = (int16)(0.5f*(fromBuffer[s] + fromBuffer[s+1]));
    }
    return toSize;
  } else { // (fromChannel > toChannel ) { // 1 => 2
    for(unsigned int m=0, s=0; m<fromSize; m++) {
      s = m<<1;
      (*toBuffer)[s] = (*toBuffer)[s+1] = fromBuffer[m];
    }
    return fromSize<<1; // multiply by 2
  }
  return fromSize;
}



