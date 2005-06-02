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

#include "audiodriversportaudio.h"
#include "../global.h"
#include "../manager.h"

AudioDriversPortAudio::AudioDriversPortAudio (Manager* manager) {
	_manager = manager;
	mydata.urg_remain = 0;
	mydata.dataIn = NULL;
	mydata.dataOut = NULL;
	mydata.urg_data = NULL;
	this->initDevice();
}

// Destructor
AudioDriversPortAudio::~AudioDriversPortAudio (void) 
{
	closeDevice();
	Pa_Terminate ();
}

int
AudioDriversPortAudio::resetDevice (void) {
	return 1;
}

int
AudioDriversPortAudio::initDevice (void) {
	int err;

	err = Pa_Initialize();
	if (err != paNoError) {
		_debug ("PortAudio error in Pa_Initialize(): %s\n", Pa_GetErrorText(err));
		exit (1);
		//TODO: change exit to a clean GUI dialog (fatal).
	}
	return 1;
}

int
AudioDriversPortAudio::closeDevice (void) {
	int err = Pa_CloseStream(_stream);
	if (err != paNoError) {
		_debug ("PortAudio error in Pa_CloseStream: %s\n", Pa_GetErrorText(err));
		exit (1);
		//TODO: change exit to a clean GUI dialog (fatal).
	}
	return 1;
}

bool
AudioDriversPortAudio::openDevice (void) {
	int err = Pa_OpenDefaultStream (
	&_stream,       	/* passes back stream pointer */
	2,              	/* input channel */
	2,              	/* output channel */
	paFloat32,     		/* 32 bit float output */
	SAMPLING_RATE,  	/* sample rate */
	FRAME_PER_BUFFER, 	/* frames per buffer */
	audioCallback,  	/* specify our custom callback */
	&mydata);         	/* pass our data through to callback */  

	if (err != paNoError) {
		_debug ("PortAudio error in Pa_OpenDefaultStream: %s\n", Pa_GetErrorText(err));
		exit (1);
		//TODO: change exit to a clean GUI dialog (fatal).
	}
	return true;
}


int 
AudioDriversPortAudio::readBuffer (void *ptr, int bytes) {
	(void) ptr;
	(void) bytes;
	return 1;
}

int
AudioDriversPortAudio::writeBuffer (void *ptr, int len) {
	(void) ptr;
	(void) len;
	return 1;
}

int
AudioDriversPortAudio::startStream(void) 
{
	int err;
	if (!Pa_IsStreamActive(_stream)) {
		err = Pa_StartStream (_stream);
		if( err != paNoError ) {
			_debug ("PortAudio error in Pa_StartStream: %s\n", 
					Pa_GetErrorText(err));
			return 0;
		}
	}
	
	return 1;
}
	
int
AudioDriversPortAudio::stopStream(void) 
{
	int err;
	
	if (!Pa_IsStreamStopped(_stream)) {
		err = Pa_StopStream (_stream);
		if( err != paNoError ) {
			_debug ("PortAudio error in Pa_StopStream: %s\n", 
					Pa_GetErrorText(err));
			return 0;
		}
	}
	
	return 1;
}

void
AudioDriversPortAudio::sleep(int msec) 
{
	Pa_Sleep(msec);
}

int
AudioDriversPortAudio::isStreamActive (void) 
{
	return Pa_IsStreamActive (_stream);
}

int
AudioDriversPortAudio::isStreamStopped (void) 
{
	return Pa_IsStreamStopped (_stream);
}

int
AudioDriversPortAudio::getDeviceCount (void)
{
	return Pa_GetDeviceCount();
}

int
AudioDriversPortAudio::audioCallback (const void *inputBuffer, 
									  void *outputBuffer,
									  unsigned long framesPerBuffer, 
									  const PaStreamCallbackTimeInfo* timeInfo,
									  PaStreamCallbackFlags statusFlags, 
									  void *userData) { 
	(void) timeInfo;
	(void) statusFlags;
	
	float32 *in  = (float32 *) inputBuffer;
	float32 *out = (float32 *) outputBuffer;

	paData* data = (paData*) userData;
	int i;
	int it;

	/* Fill output buffer */
	for (i = 0; i < (int)framesPerBuffer; i++) { 
		it = 2*i;
		if (data->urg_remain - i > 0) {
			out[it] = out[it+1] = data->urg_ptr[i];  
		} else {
			out[it] = out[it+1] = data->dataOut[i];
		}
	}

	if ((data->urg_remain -i) > 0) {
		data->urg_ptr +=i;
		data->urg_remain-= i;
	} else {
		data->urg_remain = 0;
	}
	
	/* Read input buffer */
	if (data->dataIn != NULL) {
		memcpy (data->dataIn, in, 2*sizeof(float32) * framesPerBuffer);
	}
	
	return paContinue;
}
#endif // defined(AUDIO_PORTAUDIO)

