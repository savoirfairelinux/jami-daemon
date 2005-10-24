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
	if (Manager::instance().getConfigInt(SIGNALISATION,SYMMETRIC)) { 
		_symmetric = true;
	} else {
		_symmetric = false;
	}

	_RTXThread = new AudioRtpRTX (ca, Manager::instance().getAudioDriver(), 
				      _symmetric);
	
	// Start PortAudio
	//Manager::instance().getAudioDriver()->flushMic();
	//Manager::instance().getAudioDriver()->startStream();
	
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

  if (!_sym) {
    _sessionRecv = new ost::RTPSession (local_ip, _ca->getLocalAudioPort());
    _sessionSend = new ost::RTPSession (local_ip);
    _session = NULL;
  } else {
    _debug("Symmetric RTP Session on local: %s:%d\n", localipConfig.c_str(), _ca->getLocalAudioPort());
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
		//_sessionRecv->setSchedulingTimeout (10000);
		_sessionRecv->setExpireTimeout(1000000);
		
		_sessionSend->setSchedulingTimeout(10000);
		_sessionSend->setExpireTimeout(1000000);
	} else {
		_session->setSchedulingTimeout(10000);
		_session->setExpireTimeout(1000000);
	}

	if (!_sym) {
		if (!_sessionSend->addDestination (remote_ip, 
					(unsigned short) _ca->getRemoteSdpAudioPort())) {
			_debug("RTX send: could not connect to port %d\n",  
					_ca->getRemoteSdpAudioPort());
			return;
		}
    _debug("RTP(Send): Added sessionSend destination %s:%d\n", 
        remote_ip.getHostname(), (unsigned short) _ca->getRemoteSdpAudioPort());

    //setPayloadFormat(StaticPayloadFormat(sptPCMU));
    //_debug("Payload Format: %d\n", _ca->payload);
		_sessionRecv->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _ca->payload));
		_sessionSend->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _ca->payload));

    setCancel(cancelImmediate);
    _sessionSend->setMark(true);

  } else {

    _debug("RTP(Send): Added session destination %s:%d\n", 
        remote_ip.getHostname(), (unsigned short) _ca->getRemoteSdpAudioPort());

    if (!_session->addDestination (remote_ip, (unsigned short) _ca->getRemoteSdpAudioPort())) {
      return;
    }

    _session->setPayloadFormat(ost::StaticPayloadFormat((ost::StaticPayloadType) _ca->payload));
    setCancel(cancelImmediate);
  }

  Manager::instance().getAudioDriver()->flushMic();
  Manager::instance().getAudioDriver()->flushMain();
  _debug("== AudioRtpRTX::initAudioRtpSession end == \n");
}

void
// AudioRtpRTX::sendSessionFromMic (unsigned char* data_to_send, int16* data_from_mic, int16* data_from_mic_tmp, int timestamp, int micVolume)
AudioRtpRTX::sendSessionFromMic (unsigned char* data_to_send, int16* data_from_mic, int16* data_from_mic_tmp, int timestamp)
{
	int k; 
	int compSize; 
	
  // Control volume for micro
  int availFromMic = Manager::instance().getAudioDriver()->micRingBuffer().AvailForGet(); 
  int bytesAvail;
  if (availFromMic < (int)RTP_FRAMES2SEND) { 
    bytesAvail = availFromMic; 
  } else {
    bytesAvail = (int)RTP_FRAMES2SEND;
  }

  // Get bytes from micRingBuffer to data_from_mic
  Manager::instance().getAudioDriver()->micRingBuffer().Get(data_from_mic, SAMPLES_SIZE(bytesAvail), 100);
  // control volume and stereo->mono
  for (int j = 0; j < RTP_FRAMES2SEND; j++) {
    k = j<<1;
    data_from_mic_tmp[j] = (int16)(0.5f*(data_from_mic[k] + data_from_mic[k+1]));
    //micVolume/100);
  }

  if ( _ca != NULL ) {
    // Encode acquired audio sample
    AudioCodec* ac = _ca->getAudioCodec();
    if ( ac != NULL ) {
      compSize =  ac->codecEncode (data_to_send, data_from_mic_tmp, RTP_FRAMES2SEND*2);
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
AudioRtpRTX::receiveSessionForSpkr (int16* data_for_speakers, 
//		int16* data_for_speakers_tmp, int spkrVolume, int& countTime)
     int16* data_for_speakers_tmp, int& countTime)
{
	int k;
	const ost::AppDataUnit* adu = NULL;

  // Get audio data stream
  if (!_sym) {
    adu = _sessionRecv->getData(_sessionRecv->getFirstTimestamp());
  } else {
    adu = _session->getData(_session->getFirstTimestamp());
  }
  if (adu == NULL) {
    Manager::instance().getAudioDriver()->flushMain();
    return;
  }

  int payload = adu->getType();
  unsigned char* data  = (unsigned char*)adu->getData();
  unsigned int size    = adu->getSize();

	// Decode data with relevant codec
	int expandedSize = 0;
	AudioCodec* ac = _codecBuilder.alloc(payload, "");
  if (ac != NULL) {
    expandedSize = ac->codecDecode (data_for_speakers, data, size);
	}
  ac = NULL;

  // control volume for speakers and mono->stereo
  for (int j = 0; j < expandedSize; j++) {
    k = j<<1; // fast multiply by two
    data_for_speakers_tmp[k] = data_for_speakers_tmp[k+1] = data_for_speakers[j];
    // * spkrVolume/100;
  }

  // If the current call is the call which is answered
  // Set decoded data to sound device
  Manager::instance().getAudioDriver()->putMain(data_for_speakers_tmp, SAMPLES_SIZE(RTP_FRAMES2SEND));
  //}
	
	// Notify (with a beep) an incoming call when there is already a call 
	countTime += time->getSecond();
	if (Manager::instance().incomingCallWaiting() > 0) {
		countTime = countTime % 2000; // more often...
		if (countTime < 100 and countTime > 0) {
			Manager::instance().notificationIncomingCall();
		}
	}
	Manager::instance().getAudioDriver()->startStream();
	
	delete adu; adu = NULL;
}

void
AudioRtpRTX::run (void) {
//	int micVolume;
//	int spkrVolume;
	unsigned char	*data_to_send;
	int16			*data_from_mic;
	int16			*data_from_mic_tmp;
	int				 timestamp;
	int16			*data_for_speakers = NULL;
	int16			*data_for_speakers_tmp = NULL;
	int              countTime = 0;
	
	data_from_mic = new int16[SIZEDATA]; 
	data_from_mic_tmp = new int16[SIZEDATA];
	data_to_send = new unsigned char[SIZEDATA];
	data_for_speakers = new int16[SIZEDATA];
	data_for_speakers_tmp = new int16[SIZEDATA*2];

	// Init the session
	initAudioRtpSession();
	
	timestamp = 0;

	// TODO: get frameSize from user config 
	int frameSize = 20; // 20ms frames
	TimerPort::setTimer(frameSize);

  // flush stream:
  ManagerImpl& manager = Manager::instance();
  AudioLayer *audiolayer = manager.getAudioDriver();
  audiolayer->urgentRingBuffer().flush();

	// start running the packet queue scheduler.
  //_debug("Thread: start session of AudioRtpRTX\n");
	if (!_sym) {
		_sessionRecv->startRunning();
		_sessionSend->startRunning();
	} else {
		_session->startRunning();
	}

	while (!testCancel() && _ca != NULL && _ca->enable_audio != -1) {
		//micVolume = manager.getMicVolume();
	  //spkrVolume = manager.getSpkrVolume();

		////////////////////////////
		// Send session
		////////////////////////////
    //sendSessionFromMic(data_to_send, data_from_mic, data_from_mic_tmp, timestamp, micVolume);
		sendSessionFromMic(data_to_send, data_from_mic, data_from_mic_tmp, timestamp);
		
		timestamp += RTP_FRAMES2SEND;

		////////////////////////////
		// Recv session
		////////////////////////////
    //receiveSessionForSpkr(data_for_speakers, data_for_speakers_tmp, spkrVolume, countTime);
    receiveSessionForSpkr(data_for_speakers, data_for_speakers_tmp, countTime);
		
		// Let's wait for the next transmit cycle
		Thread::sleep(TimerPort::getTimer());
		TimerPort::incTimer(frameSize); // 'frameSize' ms
	}

	delete [] data_for_speakers_tmp; data_for_speakers_tmp = 0;
  delete [] data_for_speakers;     data_for_speakers     = 0;
	delete [] data_to_send;          data_to_send          = 0;
	delete [] data_from_mic_tmp;     data_from_mic_tmp     = 0;
	delete [] data_from_mic;         data_from_mic         = 0;

  audiolayer->stopStream();
}


// EOF
