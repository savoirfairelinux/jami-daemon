/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
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
#include <string.h>

#include "audiolayer.h"
#include "../error.h"
#include "../global.h"
#include "../manager.h"

using namespace std;

AudioLayer::AudioLayer () 
  : _urgentRingBuffer(SIZEBUF)
  , _mainSndRingBuffer(SIZEBUF)
  , _micRingBuffer(SIZEBUF)
  , _stream(NULL)
{
  portaudio::System::initialize();
}

// Destructor
AudioLayer::~AudioLayer (void) 
{
  closeStream();
}

void
AudioLayer::closeStream (void) 
{
  if(_stream) {
    _stream->close();
    delete _stream;
  }
}

void
AudioLayer::openDevice (int outputIndex, int inputIndex) 
{
  closeStream();
  // Set up the parameters required to open a (Callback)Stream:
  portaudio::DirectionSpecificStreamParameters 
    outParams(portaudio::System::instance().deviceByIndex(outputIndex), 
	      2, portaudio::INT16, true, 
	      portaudio::System::instance().deviceByIndex(outputIndex).defaultLowOutputLatency(), 
	      NULL);
	
  portaudio::DirectionSpecificStreamParameters 
    inParams(portaudio::System::instance().deviceByIndex(inputIndex), 
	     2, portaudio::INT16, true, 
	     portaudio::System::instance().deviceByIndex(inputIndex).defaultLowInputLatency(), 
	     NULL);
	 
  portaudio::StreamParameters const params(inParams, outParams, 
					   SAMPLING_RATE, FRAME_PER_BUFFER, paNoFlag);
		  
  // Create (and open) a new Stream, using the AudioLayer::audioCallback
  _stream = new portaudio::MemFunCallbackStream<AudioLayer>(params, 
							    *this, 
							    &AudioLayer::audioCallback);
}

void
AudioLayer::startStream(void) 
{
  if (Manager::instance().isDriverLoaded()) {
    if (_stream && !_stream->isActive()) {
      _stream->start();
    }
  } 
}
	
void
AudioLayer::stopStream(void) 
{
  if (Manager::instance().isDriverLoaded()) {
    if (_stream && !_stream->isStopped()) {
      _stream->stop();
    }
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
  if(_stream && _stream->isActive()) {
    return true;
  }
  else {
    return false;
  }
}

bool
AudioLayer::isStreamStopped (void) 
{
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
	int toGet, toPut, urgentAvail, normalAvail, micAvailPut;
	
	urgentAvail = _urgentRingBuffer.AvailForGet();
	if (urgentAvail > 0) {  
	// Urgent data (dtmf, incoming call signal) come first.		
		if (urgentAvail < (int)framesPerBuffer) {
			toGet = urgentAvail;
		} else {
			toGet = framesPerBuffer;
		}
		_urgentRingBuffer.Get(out, SAMPLES_SIZE(toGet));
		
		// Consume the regular one as well (same amount of bytes)
		_mainSndRingBuffer.Discard(SAMPLES_SIZE(toGet));
		
	}  
	else {
	// If nothing urgent, play the regular sound samples
		normalAvail = _mainSndRingBuffer.AvailForGet();
		toGet = (normalAvail < (int)framesPerBuffer) ? normalAvail : 
			framesPerBuffer;
		_mainSndRingBuffer.Get(out, SAMPLES_SIZE(toGet));
	}

	// Additionally handle the mike's audio stream 
	micAvailPut = _micRingBuffer.AvailForPut();
	toPut = (micAvailPut <= (int)framesPerBuffer) ? micAvailPut : 
			framesPerBuffer;
	_micRingBuffer.Put(in, SAMPLES_SIZE(toPut));
   	
	return paContinue;  
}

#endif // defined(AUDIO_PORTAUDIO)

