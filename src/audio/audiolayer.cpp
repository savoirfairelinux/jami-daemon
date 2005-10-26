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

#ifdef AUDIO_PORTAUDIO

#include <stdio.h>
#include <stdlib.h>

#include "audiolayer.h"
#include "../error.h"
#include "../global.h"
#include "../manager.h"

AudioLayer::AudioLayer()
  : _urgentRingBuffer(SIZEBUF)
  , _mainSndRingBuffer(SIZEBUF)
  , _micRingBuffer(SIZEBUF)
  , _stream(NULL)
{
  _debugInit("   portaudio initialization...");
  portaudio::System::initialize();
  _debugInit("   portaudio end initialization.");
  NBCHARFORTWOINT16 = sizeof(int16)/sizeof(unsigned char) * CHANNELS;
}

// Destructor
AudioLayer::~AudioLayer (void) 
{
  portaudio::System::terminate();
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

void
AudioLayer::listDevices()
{
  ost::MutexLock guard(_mutex);
  portaudio::System::DeviceIterator pos = portaudio::System::instance().devicesBegin();
  while(pos != portaudio::System::instance().devicesEnd()) {
    _debug("AudioLayer: Device (%d) %s\n", pos->index(), pos->name());
    pos++;
  }

}

void
AudioLayer::openDevice (int index) 
{
  ost::MutexLock guard(_mutex);
  closeStream();
  // Set up the parameters required to open a (Callback)Stream:
  portaudio::DirectionSpecificStreamParameters 
    outParams(portaudio::System::instance().deviceByIndex(index), 
	      2, portaudio::INT16, true, 
	      portaudio::System::instance().deviceByIndex(index).defaultLowOutputLatency(), 
	      NULL);
	
  portaudio::DirectionSpecificStreamParameters 
    inParams(portaudio::System::instance().deviceByIndex(index), 
	     2, portaudio::INT16, true, 
	     portaudio::System::instance().deviceByIndex(index).defaultLowInputLatency(), 
	     NULL);

	
  // we could put paFramesPerBufferUnspecified instead of FRAME_PER_BUFFER to be variable
  portaudio::StreamParameters const params(inParams, outParams, 
					   SAMPLING_RATE, paFramesPerBufferUnspecified, paNoFlag /*paPrimeOutputBuffersUsingStreamCallback | paNeverDropInput*/);
		  
  // Create (and open) a new Stream, using the AudioLayer::audioCallback
  _stream = new portaudio::MemFunCallbackStream<AudioLayer>(params, 
							    *this, 
							    &AudioLayer::audioCallback);
}

void
AudioLayer::startStream(void) 
{
  ost::MutexLock guard(_mutex);
  if (_stream && !_stream->isActive()) {
    _debug("Thread: start audiolayer stream\n");
    _stream->start();
  }
}
	
void
AudioLayer::stopStream(void) 
{
  ost::MutexLock guard(_mutex);
  if (_stream && !_stream->isStopped()) {
    _debug("Thread: stop audiolayer stream\n");
      _stream->stop();
    _mainSndRingBuffer.flush();
  }
}

void
AudioLayer::sleep(int msec) 
{
  portaudio::System::instance().sleep(msec);
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

void
AudioLayer::putMain(void* buffer, int toCopy)
{
  ost::MutexLock guard(_mutex);
  int a = _mainSndRingBuffer.AvailForPut();
  if ( a >= toCopy ) {
    _mainSndRingBuffer.Put(buffer, toCopy);
  } else {
    _mainSndRingBuffer.Put(buffer, a);
  }
}

void
AudioLayer::flushMain()
{
  ost::MutexLock guard(_mutex);
  _mainSndRingBuffer.flush();
}

void
AudioLayer::putUrgent(void* buffer, int toCopy)
{
  ost::MutexLock guard(_mutex);
  int a = _mainSndRingBuffer.AvailForPut();
  if ( a >= toCopy ) {
    _urgentRingBuffer.Put(buffer, toCopy);
  } else {
    _urgentRingBuffer.Put(buffer, a);
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
  ManagerImpl& _manager = Manager::instance();
  unsigned short spkrVolume = _manager.getSpkrVolume();
  unsigned short micVolume  = _manager.getMicVolume();

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
    Tone* tone = _manager.getTelephoneTone();
    if ( tone != 0) {
      tone->getNext(out, framesPerBuffer, spkrVolume);
    } else {
      // If nothing urgent, play the regular sound samples
      normalAvail = _mainSndRingBuffer.AvailForGet();
      toGet = (normalAvail < (int)framesPerBuffer * NBCHARFORTWOINT16) ? normalAvail : framesPerBuffer * NBCHARFORTWOINT16;

      if (toGet) {
          _mainSndRingBuffer.Get(out, toGet, spkrVolume);
        } else {
          toGet = framesPerBuffer;
          _mainSndRingBuffer.PutZero(toGet);
          _mainSndRingBuffer.Get(out, toGet, 100);
        }
      }
	}

	// Additionally handle the mic's audio stream 
  micAvailPut = _micRingBuffer.AvailForPut();
  toPut = (micAvailPut <= (int)framesPerBuffer) ? micAvailPut : framesPerBuffer * NBCHARFORTWOINT16;
  _micRingBuffer.Put(in, toPut, micVolume );

	return paContinue;
}

#endif // defined(AUDIO_PORTAUDIO)

