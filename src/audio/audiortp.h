/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

/** maximum bytes inside an incoming packet 
 *  8000 sampling/s * 20s/1000 = 160
 */
#define RTP_20S_8KHZ_MAX 160
#define RTP_20S_48KHZ_MAX 960
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
  /** When we send data, we encode it inside this buffer*/
  unsigned char* _sendDataEncoded;

  /** After that we send the data inside this buffer if there is a format conversion or rate conversion */
  /** Also use for getting mic-ringbuffer data */
  SFLDataFormat* _dataAudioLayer;

  /** Buffer for 8000hz samples in conversion */
  float32* _floatBuffer8000;
  /** Buffer for 48000hz samples in conversion */ 
  float32* _floatBuffer48000;

  /** Buffer for 8000Hz samples for mic conversion */
  int16* _intBuffer8000;

  /** Debugging output file */
  //std::ofstream _fstream;

   /** libsamplerate converter for incoming voice */
  SRC_STATE*    _src_state_spkr;

  /** libsamplerate converter for outgoing voice */
  SRC_STATE*    _src_state_mic;

  /** libsamplerate error */
  int           _src_err;

  void initAudioRtpSession(void);
  void sendSessionFromMic(int);
  void receiveSessionForSpkr(int&);
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
