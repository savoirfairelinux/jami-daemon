/**
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
  portaudio::System::initialize();
  NBCHARFORTWOINT16 = sizeof(int16)/sizeof(unsigned char) * CHANNELS;
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
AudioLayer::openDevice (int indexIn, int indexOut) 
{
  closeStream();

  try {
  // Set up the parameters required to open a (Callback)Stream:
  portaudio::DirectionSpecificStreamParameters 
    outParams(portaudio::System::instance().deviceByIndex(indexOut), 
	      2, portaudio::INT16, true, 
	      portaudio::System::instance().deviceByIndex(indexOut).defaultLowOutputLatency(), 
	      NULL);
	
  portaudio::DirectionSpecificStreamParameters 
    inParams(portaudio::System::instance().deviceByIndex(indexIn), 
	     2, portaudio::INT16, true, 
	     portaudio::System::instance().deviceByIndex(indexIn).defaultLowInputLatency(), 
	     NULL);
	
  // we could put paFramesPerBufferUnspecified instead of FRAME_PER_BUFFER to be variable
  portaudio::StreamParameters const params(inParams, outParams, 
					   SAMPLING_RATE, FRAME_PER_BUFFER /*paFramesPerBufferUnspecified*/, paNoFlag /*paPrimeOutputBuffersUsingStreamCallback | paNeverDropInput*/);
		  
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
	int toGet, toPut;
  int urgentAvail, // number of int16 right and int16 left
      normalAvail, // number of int16 right and int16 left
      micAvailPut;
  unsigned short spkrVolume = Manager::instance().getSpkrVolume();
  unsigned short micVolume  = Manager::instance().getMicVolume();

  // AvailForGet tell the number of chars inside the buffer
  // framePerBuffer are the number of int16 for one channel (left)
	urgentAvail = _urgentRingBuffer.AvailForGet();
	if (urgentAvail > 0) {  
	// Urgent data (dtmf, incoming call signal) come first.		
		toGet = (urgentAvail < (int)framesPerBuffer * NBCHARFORTWOINT16) ? urgentAvail : framesPerBuffer * NBCHARFORTWOINT16;
		_urgentRingBuffer.Get(out, toGet, spkrVolume);
		
		// Consume the regular one as well (same amount of bytes)
		_mainSndRingBuffer.Discard(toGet);
	}  
	else {
    AudioLoop* tone = Manager::instance().getTelephoneTone();
    if ( tone != 0) {
      tone->getNext(out, framesPerBuffer, spkrVolume);
    } else if ( (tone=Manager::instance().getTelephoneFile()) != 0 ) {
      tone->getNext(out, framesPerBuffer, spkrVolume);
    } else {
      // If nothing urgent, play the regular sound samples
      normalAvail = _mainSndRingBuffer.AvailForGet();
      toGet = (normalAvail < (int)framesPerBuffer * NBCHARFORTWOINT16) ? normalAvail : framesPerBuffer * NBCHARFORTWOINT16;

      if (toGet) {
        _mainSndRingBuffer.Get(out, toGet, spkrVolume);
      } else {
        bzero(out, framesPerBuffer * NBCHARFORTWOINT16);
      }
    }
	}

	// Additionally handle the mic's audio stream 
  micAvailPut = _micRingBuffer.AvailForPut();
  toPut = (micAvailPut <= (int)framesPerBuffer) ? micAvailPut : framesPerBuffer * NBCHARFORTWOINT16;
  _micRingBuffer.Put(in, toPut, micVolume );

	return paContinue;
}


