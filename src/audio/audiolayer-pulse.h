/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#ifndef _AUDIO_LAYER_PULSE_H
#define _AUDIO_LAYER_PULSE_H

#include <cc++/thread.h> // for ost::Mutex


#include "../global.h"
#include "ringbuffer.h"
#include "audiodevice.h"

#define FRAME_PER_BUFFER	160

class RingBuffer;
class ManagerImpl;

class PulseLayer {
	public:
		PulseLayer(ManagerImpl* manager);
		~PulseLayer(void);

		/*
		 * @param indexIn
		 * @param indexOut
		 * @param sampleRate
		 * @param frameSize
		 */
		void openDevice(int, int, int, int);
		void startStream(void);
		void stopStream(void);
		void sleep(int);
		bool hasStream(void);
		bool isStreamActive(void);
		bool isStreamStopped(void);

		void flushMain();
		int putMain(void* buffer, int toCopy);
		int putUrgent(void* buffer, int toCopy);
		int canGetMic();
		int getMic(void *, int);
		void flushMic();

		int audioCallback ();



		void setErrorMessage(const std::string& error) { _errorMessage = error; }
		std::string getErrorMessage() { return _errorMessage; }

		/**
		 * Get the sample rate of PulseLayer
		 * accessor only
		 */
		unsigned int getSampleRate() { return _sampleRate; }
		unsigned int getFrameSize() { return _frameSize; }

		/**
		 * Toggle echo testing on/off
		 */
		void toggleEchoTesting();

	private:
		void closeStream (void);
		RingBuffer _urgentRingBuffer;
		RingBuffer _mainSndRingBuffer;
		RingBuffer _micRingBuffer;
		ManagerImpl* _manager; // augment coupling, reduce indirect access
		// a PulseLayer can't live without manager

		pa_stream* playback;
		pa_stream* record;

		/**
		 * Sample Rate SFLphone should send sound data to the sound card 
		 * The value can be set in the user config file- now: 44100HZ
		 */
		unsigned int _sampleRate;
		
		/**
 		 * Length of the sound frame we capture or read in ms
 		 * The value can be set in the user config file - now: 20ms
 		 */	 		
		unsigned int _frameSize;
		
		/**
		 * Input channel (mic) should be 1 mono
		 */
		unsigned int _inChannel; // mic

		/**
		 * Output channel (stereo) should be 1 mono
		 */
		unsigned int _outChannel; // speaker

		/**
		 * Default volume for incoming RTP and Urgent sounds.
		 */
		//unsigned short _defaultVolume; // 100
		pa_volume_t _defaultVolume;

		/**
		 * Echo testing or not
		 */
		bool _echoTesting;

		std::string _errorMessage;
		ost::Mutex _mutex;

		float *table_;
		int tableSize_;
		int leftPhase_;
};

#endif // _AUDIO_LAYER_PULSE_H_

