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
#include <iostream>
#include <string>

#include "../global.h"
#include "../manager.h"
#include "audiocodec.h"
#include "audiortp.h"
#include "audiolayer.h"
#include "codecDescriptor.h"
#include "ringbuffer.h"
#include "../user_cfg.h"
#include "../sipcall.h"
#include "../../stund/stun.h"

using namespace ost;
using namespace std;


////////////////////////////////////////////////////////////////////////////////
// AudioRtp                                                          
////////////////////////////////////////////////////////////////////////////////
AudioRtp::AudioRtp () {
	_RTXThread = NULL;
}

AudioRtp::~AudioRtp (void) {
  terminate();
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
	Manager::instance().getAudioDriver()->micRingBuffer().flush();
	Manager::instance().getAudioDriver()->startStream();
	
  _debug("AudioRtp::createNewSession: starting RTX thread\n");
	if (_RTXThread->start() != 0) {
		return -1;
	}
		
	return 0;
}

	
void
AudioRtp::closeRtpSession (SipCall *ca) {
	// This will make RTP threads finish.
	if (ca->enable_audio > 0) {
		ca->enable_audio = -1;

		if (_RTXThread != NULL) {
      _debug("Thread: stop AudioRTP for sipcall: %d\n", ca->getId());
			delete _RTXThread; _RTXThread = NULL;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// AudioRtpRTX Class                                                          //
////////////////////////////////////////////////////////////////////////////////
AudioRtpRTX::AudioRtpRTX (SipCall *sipcall, 
			  AudioLayer* driver, 
			  bool sym) {
	time = new Time();
	_ca = sipcall;
	_sym = sym;
	_audioDevice = driver;

	// TODO: Change bind address according to user settings.
  std::string localipConfig = _ca->getLocalIp();
	InetHostAddress local_ip(localipConfig.c_str());

	_debug("RTP: listening on IP %s local port : %d\n", localipConfig.c_str(), _ca->getLocalAudioPort());
	if (!_sym) {
		_sessionRecv = new RTPSession (local_ip, _ca->getLocalAudioPort());
		_sessionSend = new RTPSession (local_ip);
	} else {
		_session = new SymmetricRTPSession (local_ip,  _ca->getLocalAudioPort());
	}
}

AudioRtpRTX::~AudioRtpRTX () {
  terminate();
  _debug("Thread: AudioRtpRTX stop session\n");
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
	InetHostAddress remote_ip(_ca->getRemoteSdpAudioIp());
	
	if (!remote_ip) {
	   _debug("RTP: Target IP address [%s] is not correct!\n", _ca->getRemoteSdpAudioIp());
	   return;
	} else {
		_debug("RTP: Sending to %s : %d\n", _ca->getRemoteSdpAudioIp(), _ca->getRemoteSdpAudioPort());
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
		} else {
			_debug("RTP(Send): Added destination %s : %d\n", 
					remote_ip.getHostname(), 
					(unsigned short) _ca->getRemoteSdpAudioPort());
		}

    //setPayloadFormat(StaticPayloadFormat(sptPCMU));
    _debug("Payload Format: %d\n", _ca->payload);
		_sessionRecv->setPayloadFormat(StaticPayloadFormat((StaticPayloadType) _ca->payload));
		_sessionSend->setPayloadFormat(StaticPayloadFormat((StaticPayloadType) _ca->payload));

		setCancel(cancelImmediate);
		_sessionSend->setMark(true);

	} else {
		if (!_session->addDestination (remote_ip, (unsigned short) _ca->getRemoteSdpAudioPort())) {
			return;
		} else {
			_session->setPayloadFormat(StaticPayloadFormat((StaticPayloadType) _ca->payload));
			setCancel(cancelImmediate);
		}
	}
	_debug("-----------------------\n");
}

void
AudioRtpRTX::sendSessionFromMic (unsigned char* data_to_send, int16* data_from_mic, int16* data_from_mic_tmp, int timestamp, int micVolume)
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
  Manager::instance().getAudioDriver()->micRingBuffer().Get(data_from_mic, 
      SAMPLES_SIZE(bytesAvail));
  // control volume and stereo->mono
  for (int j = 0; j < RTP_FRAMES2SEND; j++) {
    k = j*2;
    data_from_mic_tmp[j] = (int16)(0.5f*(data_from_mic[k] +
                      data_from_mic[k+1]) * 
                      micVolume/100); 
  }

	// Encode acquired audio sample
	compSize = _ca->getAudioCodec()->codecEncode (data_to_send,
												  data_from_mic_tmp, 
												  RTP_FRAMES2SEND*2);
	// Send encoded audio sample over the network
	if (!_sym) {
		_sessionSend->putData(timestamp, data_to_send, compSize);
	} else {
		_session->putData(timestamp, data_to_send, compSize);
	}

}

void
AudioRtpRTX::receiveSessionForSpkr (int16* data_for_speakers, 
		int16* data_for_speakers_tmp, int spkrVolume, int& countTime)
{
	int expandedSize;
	int k;
	const AppDataUnit* adu = NULL;

  // Get audio data stream
  if (!_sym) {
    adu = _sessionRecv->getData(_sessionRecv->getFirstTimestamp());
  } else {
    adu = _session->getData(_session->getFirstTimestamp());
  }
  if (adu == NULL) {
    Manager::instance().getAudioDriver()->mainSndRingBuffer().flush();
    Manager::instance().getAudioDriver()->stopStream();
    return;
  }

	// Decode data with relevant codec
	CodecDescriptor* cd = new CodecDescriptor (adu->getType());
	
	AudioCodec* ac = cd->alloc(adu->getType(), "");

	expandedSize = ac->codecDecode (data_for_speakers,
									(unsigned char*) adu->getData(),
									adu->getSize());
		
	// control volume for speakers and mono->stereo
	for (int j = 0; j < expandedSize; j++) {
		k = j*2;
		data_for_speakers_tmp[k] = data_for_speakers_tmp[k+1]= 
			data_for_speakers[j] * spkrVolume/100;
	}

	// If the current call is the call which is answered
	//if (Manager::instance().isCurrentId(_ca->getId())) {
		// Set decoded data to sound device
		Manager::instance().getAudioDriver()->putMain(data_for_speakers_tmp, SAMPLES_SIZE(RTP_FRAMES2SEND));
	//}
	
	// Notify (with a beep) an incoming call when there is already a call 
	countTime += time->getSecond();
	if (Manager::instance().incomingCallWaiting() > 0) {
		countTime = countTime % 4000;
		if (countTime < 100 and countTime > 0) {
			Manager::instance().notificationIncomingCall();
		}
	}

	Manager::instance().getAudioDriver()->startStream();
	
  delete ac;  ac = NULL;
	delete cd;  cd = NULL;
	delete adu; adu = NULL;
}

void
AudioRtpRTX::run (void) {
	int micVolume;
	int spkrVolume;
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
  audiolayer->mainSndRingBuffer().flush();
  audiolayer->urgentRingBuffer().flush();

	// start running the packet queue scheduler.
  _debug("Thread: start session of AudioRtpRTX\n");
	if (!_sym) {
		_sessionRecv->startRunning();
		_sessionSend->startRunning();
	} else {
		_session->startRunning();
	}

	while (!testCancel() && _ca->enable_audio != -1) {
		micVolume = manager.getMicVolume();
	  spkrVolume = manager.getSpkrVolume();

		////////////////////////////
		// Send session
		////////////////////////////
		sendSessionFromMic(data_to_send, data_from_mic, data_from_mic_tmp,
				timestamp, micVolume);
		
		timestamp += RTP_FRAMES2SEND;

		////////////////////////////
		// Recv session
		////////////////////////////
		receiveSessionForSpkr(data_for_speakers, data_for_speakers_tmp,
				spkrVolume, countTime);
		
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
  audiolayer->mainSndRingBuffer().flush();
  //audiolayer->urgentRingBuffer().flush();
}


// EOF
