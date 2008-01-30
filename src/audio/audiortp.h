/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __AUDIO_RTP_H__
#define __AUDIO_RTP_H__

#include <cstdio>
#include <cstdlib>
#include <fstream> // fstream + iostream for _fstream debugging...
#include <iostream>

#include <ccrtp/rtp.h>
#include <cc++/numbers.h>

#include <samplerate.h>

#include "../global.h"

#define UP_SAMPLING 0
#define DOWN_SAMPLING 1


class SIPCall;

///////////////////////////////////////////////////////////////////////////////
// Two pair of sockets
///////////////////////////////////////////////////////////////////////////////
class AudioRtpRTX : public ost::Thread, public ost::TimerPort {
	public:
		AudioRtpRTX (SIPCall *, bool);
		~AudioRtpRTX();

		ost::Time *time; 	// For incoming call notification 
		virtual void run ();

	private:
		SIPCall* _ca;
		ost::RTPSession *_sessionSend;
		ost::RTPSession *_sessionRecv;
		ost::SymmetricRTPSession *_session;
		ost::Semaphore _start;
		bool _sym;

		/** When we receive data, we decode it inside this buffer */
		int16* _receiveDataDecoded;
		/** Buffers used for send data from the mic */
		unsigned char* _sendDataEncoded;
		int16* _intBufferDown;

		/** After that we send the data inside this buffer if there is a format conversion or rate conversion */
		/** Also use for getting mic-ringbuffer data */
		SFLDataFormat* _dataAudioLayer;

		/** Buffers used for sample rate conversion */
		float32* _floatBufferDown;
		float32* _floatBufferUp;

		/** Debugging output file */
		//std::ofstream _fstream;

		/** libsamplerate converter for incoming voice */
		SRC_STATE*    _src_state_spkr;
		/** libsamplerate converter for outgoing voice */
		SRC_STATE*    _src_state_mic;
		/** libsamplerate error */
		int           _src_err;

		// Variables to process audio stream
		int _layerSampleRate;  // sample rate for playing sound (typically 44100HZ)
		int _codecSampleRate; // sample rate of the codec we use to encode and decode (most of time 8000HZ)
		int _layerFrameSize; // length of the sound frame we capture in ms(typically 20ms)

		void initAudioRtpSession(void);
		/**
 		 * Get the data from the mic, encode it and send it through the RTP session
 		 */ 		 	
		void sendSessionFromMic(int);
		/**
 		 * Get the data from the RTP packets, decode it and send it to the sound card
 		 */		 	
		void receiveSessionForSpkr(int&);
		/**
 		 * Init the buffers used for processing sound data
 		 */ 
		void initBuffers(void);
		/**
 		 * Call the appropriate function, up or downsampling
 		 */ 
		int reSampleData(int, int ,int);
		/**
 		 * Upsample the data from the clock rate of the codec to the sample rate of the layer
 		 * @param int The clock rate of the codec
 		 * @param int The number of samples we have in the buffer
 		 * @return int The number of samples after the operation
 		 */
		int upSampleData(int, int);
		/**
 		 * Downsample the data from the sample rate of the layer to the clock rate of the codec
 		 *  @param int The clock rate of the codec
 		 *  @param int The number of samples we have in the buffer 
 		 *  @return int The number of samples after the operation
 		 */
		int downSampleData(int, int);

		/** Pointer on function to handle codecs **/
		void* handle_codec;
		
		/**
 		 * Load dynamically a codec (.so library) 
 		 * @param payload The payload of the codec you want to load
 		 * @return AudioCodec* A pointer on a audio codec object
 		 */
		AudioCodec* loadCodec(int payload);
		
		/**
 		 * Destroy and close dynamically a codec (.so library) 
 		 * @param audiocodec The audio codec you want to unload
 		 */
		void unloadCodec(AudioCodec* audiocodec);
};

///////////////////////////////////////////////////////////////////////////////
// Main class rtp
///////////////////////////////////////////////////////////////////////////////
class AudioRtp {
	public:
		AudioRtp();
		~AudioRtp();

		int 			createNewSession (SIPCall *);
		void			closeRtpSession	 ();

	private:
		AudioRtpRTX*	        _RTXThread;
		bool			_symmetric;
		ost::Mutex            _threadMutex;
};

#endif // __AUDIO_RTP_H__
