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
#include "ringbuffer.h"
#include "../error.h"
#include "../global.h"
#include "../manager.h"

using namespace std;

AudioLayer::AudioLayer (Manager* manager) {
	_manager = manager;
	initDevice();

	_urgentRingBuffer = new RingBuffer(SIZEBUF);
	_mainSndRingBuffer = new RingBuffer(SIZEBUF);
	_micRingBuffer = new RingBuffer(SIZEBUF);
}

// Destructor
AudioLayer::~AudioLayer (void) 
{
	closeStream();
	autoSys->terminate();
	
	delete autoSys;
	delete _urgentRingBuffer;
	delete _mainSndRingBuffer;
	delete _micRingBuffer;

}

void
AudioLayer::initDevice (void) {
	autoSys = new portaudio::AutoSystem();
	autoSys->initialize();
} 

void
AudioLayer::closeStream (void) {
    _stream->close();
}

void
AudioLayer::openDevice (int index) 
{
	// Set up the System:
	portaudio::System &sys = portaudio::System::instance();

	// Set up the parameters required to open a (Callback)Stream:
	portaudio::DirectionSpecificStreamParameters outParams(
			sys.deviceByIndex(index), 2, portaudio::INT16, true, 
			sys.deviceByIndex(index).defaultLowOutputLatency(), NULL);
	
	portaudio::DirectionSpecificStreamParameters inParams(
			sys.deviceByIndex(index), 2, portaudio::INT16, true, 
			sys.deviceByIndex(index).defaultLowInputLatency(), NULL);
	 
	portaudio::StreamParameters const params(inParams, outParams, 
			SAMPLING_RATE, FRAME_PER_BUFFER, paNoFlag);
		  
	// Create (and open) a new Stream, using the AudioLayer::audioCallback
	_stream = new portaudio::MemFunCallbackStream<AudioLayer>(
			params, *this, &AudioLayer::audioCallback);
}

void
AudioLayer::startStream(void) 
{
	if (_manager->isDriverLoaded()) {
		if (!_stream->isActive()) {
			_stream->start();
		}
	} 
}
	
void
AudioLayer::stopStream(void) 
{
	if (_manager->isDriverLoaded()) {
		if (!_stream->isStopped()) {
			_stream->stop();
		}
	} 
}

void
AudioLayer::sleep(int msec) 
{
	portaudio::System &sys = portaudio::System::instance();
	sys.sleep(msec);
}

int
AudioLayer::isStreamActive (void) 
{
	return _stream->isActive();
}

int
AudioLayer::isStreamStopped (void) 
{
	return _stream->isStopped();
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
	
	urgentAvail = _urgentRingBuffer->AvailForGet();
	if (urgentAvail > 0) {  
	// Urgent data (dtmf, incoming call signal) come first.		
		if (urgentAvail < (int)framesPerBuffer) {
			toGet = urgentAvail;
		} else {
			toGet = framesPerBuffer;
		}
		_urgentRingBuffer->Get(out, SAMPLES_SIZE(toGet));
		
		// Consume the regular one as well
		_mainSndRingBuffer->Discard(SAMPLES_SIZE(toGet));
	}  
	else {
	// If nothing urgent, play the regular sound samples
		normalAvail = _mainSndRingBuffer->AvailForGet();
		toGet = (normalAvail < (int)framesPerBuffer) ? normalAvail : 
			framesPerBuffer;
		_mainSndRingBuffer->Get(out, SAMPLES_SIZE(toGet));
	}

	// Additionally handle the mike's audio stream 
	micAvailPut = _micRingBuffer->AvailForPut();
	toPut = (micAvailPut <= (int)framesPerBuffer) ? micAvailPut : 
			framesPerBuffer;
	_micRingBuffer->Put(in, SAMPLES_SIZE(toPut));
   	
	return paContinue;  
}

#endif // defined(AUDIO_PORTAUDIO)

