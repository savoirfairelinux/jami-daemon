/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include <cstdio>
#include <cstdlib>
#include <ccrtp/rtp.h>
#include <assert.h>
#include <string>

#include "../global.h"
#include "../manager.h"
#include "audiocodec.h"
#include "audiortp.h"
#include "audiolayer.h"
#include "ringbuffer.h"
#include "../user_cfg.h"
#include "../sipcall.h"
#include "../../stund/stun.h"

////////////////////////////////////////////////////////////////////////////////
// AudioRtp                                                          
////////////////////////////////////////////////////////////////////////////////
AudioRtp::AudioRtp () {
	_RTXThread = NULL;
}

AudioRtp::~AudioRtp (void) {
	delete _RTXThread; _RTXThread = NULL;
}

int 
AudioRtp::createNewSession (SipCall *ca) {
  // Start RTP Send/Receive threads
  ca->enable_audio = 1;
  _symmetric = Manager::instance().getConfigInt(SIGNALISATION,SYMMETRIC) ? true : false;
  _RTXThread = new AudioRtpRTX (ca, Manager::instance().getAudioDriver(), _symmetric);

  //_debug("AudioRtp::createNewSession: starting RTX thread\n");
  if (_RTXThread->start() != 0) {
    return -1;
  }
  return 0;
}

	
void
AudioRtp::closeRtpSession () {
  // This will make RTP threads finish.
  delete _RTXThread; _RTXThread = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AudioRtpRTX Class                                                          //
////////////////////////////////////////////////////////////////////////////////
AudioRtpRTX::AudioRtpRTX (SipCall *sipcall, AudioLayer* driver, bool sym) : _codecBuilder(0) {

  time = new ost::Time();
  _ca = sipcall;
  _sym = sym;
  _audioDevice = driver;

  // TODO: Change bind address according to user settings.
  std::string localipConfig = _ca->getLocalIp();
  ost::InetHostAddress local_ip(localipConfig.c_str());

  _debug("AudioRtpRTX ctor : Local IP:port %s:%d\tsymmetric:%d\n", local_ip.getHostname(), _ca->getLocalAudioPort(), _sym);

  if (!_sym) {
    _sessionRecv = new ost::RTPSession(local_ip, _ca->getRemoteSdpAudioPort());
    _sessionSend = new ost::RTPSession(local_ip, _ca->getLocalAudioPort());
    _session = NULL;
  } else {
    _session = new ost::SymmetricRTPSession (local_ip, _ca->getLocalAudioPort());
    _sessionRecv = NULL;
    _sessionSend = NULL;
  }
}

AudioRtpRTX::~AudioRtpRTX () {
  try {
    terminate();
  } catch (...) {
    _debug("AudioRtpRTX: try to terminate, but catch an exception...\n");
  }
  _ca = NULL;

  if (!_sym) {
    delete _sessionRecv; _sessionRecv = NULL;
    delete _sessionSend; _sessionSend = NULL;
  } else {
    delete _session;     _session = NULL;
  }

  delete time; time = NULL;
}

void
AudioRtpRTX::initAudioRtpSession (void) 
{
  ost::InetHostAddress remote_ip(_ca->getRemoteSdpAudioIp());
  if (!remote_ip) {
    _debug("RTP: Target IP address [%s] is not correct!\n", _ca->getRemoteSdpAudioIp());
    return;
  }

  // Initialization
  if (!_sym) {
    _sessionRecv->setSchedulingTimeout (10000);
    _sessionRecv->setExpireTimeout(1000000);

    _sessionSend->setSchedulingTimeout(10000);
    _sessionSend->setExpireTimeout(1000000);
  } else {
    _session->setSchedulingTimeout(10000);
    _session->setExpireTimeout(1000000);
  }

  if (!_sym) {
    std::string localipConfig = _ca->getLocalIp();
    ost::InetHostAddress local_ip(localipConfig.c_str());
    if ( !_sessionRecv->addDestination(local_ip, (unsigned short) _ca->getLocalAudioPort()) ) {
      _debug("RTX recv: could not connect to port %d\n",  _ca->getLocalAudioPort());
      return;
    }
    if (!_sessionSend->addDestination (remote_ip, (unsigned short) _ca->getRemoteSdpAudioPort())) {
      _debug("RTX send: could not connect to port %d\n",  _ca->getRemoteSdpAudioPort());
      return;
    }
    _debug("RTP(Send): Added sessionSend destination %s:%d\n", remote_ip.getHostname(), (unsigned short) _ca->getRemoteSdpAudioPort());

    //setPayloadFormat(StaticPayloadFormat(sptPCMU));
    //_debug("Payload Format: %d\n", _ca->payload);
    _sessionRecv->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _ca->payload));
    _sessionSend->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _ca->payload));

    _sessionSend->setMark(true);
    setCancel(cancelImmediate);

  } else {

    _debug("RTP(Send): Added session destination %s:%d\n", 
        remote_ip.getHostname(), (unsigned short) _ca->getRemoteSdpAudioPort());

    if (!_session->addDestination (remote_ip, (unsigned short) _ca->getRemoteSdpAudioPort())) {
      return;
    }

    _session->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _ca->payload));
    setCancel(cancelImmediate);
  }
  _debug("== AudioRtpRTX::initAudioRtpSession end == \n");
}

void
AudioRtpRTX::sendSessionFromMic (unsigned char* data_to_send, int16* data_from_mic_stereo, int16* data_from_mic_mono, int timestamp)
{
  int availBytesFromMic = Manager::instance().getAudioDriver()->micRingBuffer().AvailForGet();
  int maxBytesToGet = RTP_FRAMES2SEND * 2 * 2; // * channels * int16/byte
  int bytesAvail;

  // take the lower
  if (availBytesFromMic < maxBytesToGet) {
    bytesAvail = availBytesFromMic;
  } else {
    bytesAvail = maxBytesToGet;
  }

  // Get bytes from micRingBuffer to data_from_mic
  Manager::instance().getAudioDriver()->startStream();
  Manager::instance().getAudioDriver()->micRingBuffer().Get(data_from_mic_stereo, bytesAvail, 100);
  // control volume and stereo->mono
  // the j is in int16 RTP_FRAMES2SEND
  // data_from_mic_mono = 0 to RTP_FRAME2SEND [in int16]
  for (int j = 0, k=0; j < bytesAvail/4; j++) {
    k = j<<1;
    data_from_mic_mono[j] = (int16)(0.5f*(data_from_mic_stereo[k] + data_from_mic_stereo[k+1]));
  }
  if ( bytesAvail != maxBytesToGet ) {
    // fill end with 0...
    bzero(data_from_mic_mono + (bytesAvail/4), (maxBytesToGet-bytesAvail)/2);
  }

  if ( _ca != NULL ) {
    // Encode acquired audio sample
    AudioCodec* ac = _ca->getAudioCodec();
    if ( ac != NULL ) {
      // for the mono: range = 0 to RTP_FRAME2SEND * sizeof(int16)
      // codecEncode(char *dest, int16* src, size in bytes of the src)
      int compSize = ac->codecEncode (data_to_send, data_from_mic_mono, RTP_FRAMES2SEND*2);
      // encode divise by two
      // Send encoded audio sample over the network
      if (!_sym) {
        _sessionSend->putData(timestamp, data_to_send, compSize);
      } else {
        _session->putData(timestamp, data_to_send, compSize);
      }
    } else { _debug("No AudioCodec for the mic\n"); }
  }
}

void
AudioRtpRTX::receiveSessionForSpkr (int16* data_for_speakers_stereo, int16* data_for_speakers_recv, int& countTime)
{
  const ost::AppDataUnit* adu = NULL;
  // Get audio data stream

  if (!_sym) {
    adu = _sessionRecv->getData(_sessionRecv->getFirstTimestamp());
  } else {
    adu = _session->getData(_session->getFirstTimestamp());
  }
  if (adu == NULL) {
    //Manager::instance().getAudioDriver()->flushMain();
    return;
  }

  int payload = adu->getType();
  unsigned char* data  = (unsigned char*)adu->getData();
  unsigned int size    = adu->getSize();

	// Decode data with relevant codec
	int expandedSize = 0;
	AudioCodec* ac = _codecBuilder.alloc(payload, "");
  if (ac != NULL) {
    // codecDecode(int16 *dest, char* src, size in bytes of the src)
    // decode multiply by two
    // size shall be RTP_FRAME2SEND or lower
    expandedSize = ac->codecDecode(data_for_speakers_recv, data, size);
  }
  ac = NULL;

  // control volume for speakers and mono->stereo
  // expandedSize is in bytes for data_for_speakers_recv
  // data_for_speakers_recv are in int16
  for (int j = 0, k=0; j < expandedSize/2; j++) {
    k = j<<1; // fast multiply by two
    data_for_speakers_stereo[k] = data_for_speakers_stereo[k+1] = data_for_speakers_recv[j];
  }

  // If the current call is the call which is answered
  // Set decoded data to sound device
  // expandedSize is in mono/bytes, since we double in stereo, we send two time more
  Manager::instance().getAudioDriver()->putMain(data_for_speakers_stereo, expandedSize*2);
  //}
	
	// Notify (with a beep) an incoming call when there is already a call 
	countTime += time->getSecond();
	if (Manager::instance().incomingCallWaiting() > 0) {
		countTime = countTime % 2000; // more often...
		if (countTime < 100 and countTime > 0) {
			Manager::instance().notificationIncomingCall();
		}
	}

	delete adu; adu = NULL;
}

void
AudioRtpRTX::run (void) {
  //mic, we receive from soundcard in stereo, and we send encoded
  //encoding before sending
  int16 *data_from_mic_stereo = new int16[RTP_FRAMES2SEND*2];
  int16 *data_from_mic_mono = new int16[RTP_FRAMES2SEND];
  unsigned char *char_to_send = new unsigned char[RTP_FRAMES2SEND]; // two time more for codec

  //spkr, we receive from rtp in mono and we send in stereo
  //decoding after receiving
  int16 *data_for_speakers_recv = new int16[RTP_FRAMES2SEND];
  int16 *data_for_speakers_stereo = new int16[RTP_FRAMES2SEND*2];

  // Init the session
  initAudioRtpSession();

  // flush stream:
  ManagerImpl& manager = Manager::instance();
  AudioLayer *audiolayer = manager.getAudioDriver();

  // start running the packet queue scheduler.
  //_debug("Thread: start session of AudioRtpRTX\n");
  if (!_sym) {
    _sessionRecv->startRunning();
    _sessionSend->startRunning();
  } else {
    _session->startRunning();
    _debug("Session is now: %d active?\n", _session->isActive());
  }

  int timestamp = 0; // for mic
  int countTime = 0; // for receive
  // TODO: get frameSize from user config 
  int frameSize = 20; // 20ms frames
  TimerPort::setTimer(frameSize);

  audiolayer->flushMic();
	while (!testCancel() && _ca != NULL && _ca->enable_audio != -1) {
		////////////////////////////
		// Send session
		////////////////////////////
		sendSessionFromMic(char_to_send, data_from_mic_stereo, data_from_mic_mono, timestamp);
		timestamp += RTP_FRAMES2SEND;

		////////////////////////////
		// Recv session
		////////////////////////////
    receiveSessionForSpkr(data_for_speakers_stereo, data_for_speakers_recv, countTime);
		
		// Let's wait for the next transmit cycle
		Thread::sleep(TimerPort::getTimer());
		TimerPort::incTimer(frameSize); // 'frameSize' ms
	}

	delete [] data_for_speakers_stereo; data_for_speakers_stereo = 0;
  delete [] data_for_speakers_recv;   data_for_speakers_recv   = 0;
	delete [] char_to_send;          char_to_send          = 0;
	delete [] data_from_mic_mono;    data_from_mic_mono    = 0;
	delete [] data_from_mic_stereo;  data_from_mic_stereo  = 0;

  audiolayer->stopStream();
}


// EOF
