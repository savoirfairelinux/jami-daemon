/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
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

#include "audiortp.h"
#include "../configuration.h"
#include "../manager.h"
#include "../user_cfg.h"
#include "../sipcall.h"
#include "../stund/stun.h"

using namespace ost;
using namespace std;


////////////////////////////////////////////////////////////////////////////////
// AudioRtp                                                          
////////////////////////////////////////////////////////////////////////////////
AudioRtp::AudioRtp (Manager *manager) {
	string svr;
	
	_manager = manager;
	_RTXThread = NULL;
	
	if (!manager->useStun()) {
		if (get_config_fields_str(SIGNALISATION, PROXY).empty()) {
			svr = get_config_fields_str(SIGNALISATION, PROXY);
		}
	} else {
		svr = get_config_fields_str(SIGNALISATION, HOST_PART);
	}
}

AudioRtp::~AudioRtp (void) {
}

int 
AudioRtp::createNewSession (SipCall *ca) {
	// Start RTP Send/Receive threads
	ca->enable_audio = 1;
	if (!_manager->useStun()) { 
		_symetric = false;
	} else {
		_symetric = true;
	}
	
#ifdef ALSA
	if (_manager->useAlsa()) {
		_RTXThread = new AudioRtpRTX (ca, _manager->audiodriver, 
				_manager->audiodriverReadAlsa, _manager, _symetric);
	}
#endif
	if (!_manager->useAlsa()) {
		_RTXThread = new AudioRtpRTX (ca, _manager->audiodriver, NULL,_manager, 
			_symetric);
	}
	
	if (_RTXThread->start() != 0) {
		return -1;
	}
		
	return 0;
}

	
void
AudioRtp::closeRtpSession (SipCall *ca) {
	// This will make RTP threads finish.
	ca->enable_audio = -1;

	if (_RTXThread != NULL) {
		delete _RTXThread;
		_RTXThread = NULL;
	}

	// Flush audio read buffer
	_manager->audiodriver->resetDevice();
}

////////////////////////////////////////////////////////////////////////////////
// AudioRtpRTX Class                                                          //
////////////////////////////////////////////////////////////////////////////////
AudioRtpRTX::AudioRtpRTX (SipCall *sipcall, AudioDrivers *driver, 
		AudioDrivers *read_driver, Manager *mngr, bool sym) {
	time = new Time();
	_manager = mngr;
	_ca = sipcall;
	_sym =sym;
	_audioDevice = driver;
#ifdef ALSA
	if (_manager->useAlsa()) {
		_audioDeviceRead = read_driver;
	}
#endif

	// TODO: Change bind address according to user settings.
	InetHostAddress local_ip("0.0.0.0");

	if (!_sym) {
		_debug("Audiortp localport : %d\n", _ca->getLocalAudioPort());
		_sessionRecv = new RTPSession (local_ip, _ca->getLocalAudioPort());
		_sessionSend = new RTPSession (local_ip);
	} else {
		int forcedPort = _manager->getFirewallPort();
		_session = new SymmetricRTPSession (local_ip, forcedPort);
	}
}

AudioRtpRTX::~AudioRtpRTX () {
	terminate();
	
	if (!_sym) {
		if (_sessionRecv != NULL) {
			delete _sessionRecv;	
			_sessionRecv = NULL;
		}
		if (_sessionSend != NULL) {
			delete _sessionSend;	
			_sessionSend = NULL;
		}
	} else {
		if (_session != NULL) {
			delete _session;
			_session = NULL;
		}
	}
}

void
AudioRtpRTX::run (void) {
	unsigned char	*data_to_send;
	short			*data_mute;
	short			*data_from_mic;
	short			*data_from_mic_tmp;
	int				 i,
					 compSize, 
					 timestamp;
	int				 expandedSize;
	short			*data_for_speakers = NULL;
	int	 			 countTime = 0;
	data_for_speakers = new short[2048];
	data_from_mic = new short[1024];
	data_from_mic_tmp = new short[1024];
	data_to_send = new unsigned char[1024];
	data_mute = new short[1024];

	InetHostAddress remote_ip(_ca->getRemoteSdpAudioIp());
	
	if (!remote_ip) {
	   _debug("RTX: IP address is not correct!\n");
	   exit();
	} else {
		_debug("RTX: Connected to %s : %d\n", _ca->getRemoteSdpAudioIp(), 
				_ca->getRemoteSdpAudioPort());
	}
	
	// Initialization
	if (!_sym) {
		_sessionRecv->setSchedulingTimeout (100000);
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
			exit();
		} else {
			_debug("RTP(Send): Added destination %s : %d\n", 
					remote_ip.getHostname(), 
					(unsigned short) _ca->getRemoteSdpAudioPort());
		}

		_sessionRecv->setPayloadFormat(StaticPayloadFormat(
				(StaticPayloadType) _ca->payload));
		_sessionSend->setPayloadFormat(StaticPayloadFormat(
				(StaticPayloadType) _ca->payload));
		setCancel(cancelImmediate);
		_sessionSend->setMark(true);

	} else {
		if (!_session->addDestination (remote_ip, 
					(unsigned short) _ca->getRemoteSdpAudioPort())) {
			exit();
		} else {
			_session->setPayloadFormat(StaticPayloadFormat(
				(StaticPayloadType) _ca->payload));
			setCancel(cancelImmediate);
		}
	}
	
	timestamp = 0;

	// TODO: get frameSize from user config 
	int frameSize = 20; // 20ms frames
	TimerPort::setTimer(frameSize);
	
	// start running the packet queue scheduler.
	if (!_sym) {
		_sessionRecv->startRunning();
		_sessionSend->startRunning();	
	} else {
		_session->startRunning();
	}

	while (_ca->enable_audio != -1) {
		////////////////////////////
		// Send session
		////////////////////////////
		int size = 320;
		if (!_manager->getCall(_ca->getId())->isOnMute()) {
#ifdef ALSA
			if (_manager->useAlsa()) {
				i = _audioDeviceRead->readBuffer (data_from_mic, size);
			}
#endif
			if (!_manager->useAlsa()) {
				i = _audioDevice->readBuffer (data_from_mic, size);
			}
		} 
	else {
			// When IP-phone user click on mute button, we read buffer of a
			// temp buffer to avoid delay in sound.
#ifdef ALSA
			if (_manager->useAlsa())
				i = _audioDeviceRead->readBuffer (data_mute, size);
#endif
			if (!_manager->useAlsa())
				i = _audioDevice->readBuffer (data_mute, size);
		}
		// TODO : return an error because no sound
		if (i < 0) {
			break;
		}
		for (int j = 0; j < i; j++) {
			data_from_mic_tmp[j] = data_from_mic[j] * 
												_manager->getMicroVolume()/100;
		}
		
		// Encode acquired audio sample
		compSize = _ca->getAudioCodec()->codecEncode (data_to_send,
													  data_from_mic_tmp, 
													  i);
		// Send encoded audio sample
		if (!_sym) {
			_sessionSend->putData(timestamp, data_to_send, compSize);
		} else {
			_session->putData(timestamp, data_to_send, compSize);
		}
		timestamp += 160;

		////////////////////////////
		// Recv session
		////////////////////////////
		const AppDataUnit* adu = NULL;

		do {
			Thread::sleep(5); // in msec.
			if (!_sym) {
				adu = _sessionRecv->getData(_sessionRecv->getFirstTimestamp());
			} else {
				adu = _session->getData(_session->getFirstTimestamp());
			}
		} while (adu == NULL);

		// Decode data with relevant codec
		CodecDescriptor* cd = new CodecDescriptor (adu->getType());
		
		AudioCodec* ac = cd->alloc(adu->getType(), "");

		expandedSize = ac->codecDecode (data_for_speakers,
									    (unsigned char*) adu->getData(),
										adu->getSize());
			
		// Set decoded data to sound device
		_audioDevice->audio_buf.resize(expandedSize);
		_audioDevice->audio_buf.setData (data_for_speakers, 
										 _manager->getSpkrVolume()); 

		// Notify (with a bip) an incoming call when there is already a call 
		countTime += time->getSecond();
		if (_manager->getNumberOfCalls() > 0 and _manager->getbRingtone()) {
			countTime = countTime % 2000;
			if (countTime < 10 and countTime > 0) {
				_manager->notificationIncomingCall();
			}
		} 
	  	
		// Write data or notification
		i = _audioDevice->writeBuffer ();
		delete cd;
		delete adu;
   
		// Let's wait for the next transmit cycle
		Thread::sleep(TimerPort::getTimer());
		TimerPort::incTimer(frameSize); // 'frameSize' ms
	}
		 
//	AudioCodec::destroy(_ca->payload);
	delete[] data_for_speakers;
	delete[] data_from_mic;
	delete[] data_from_mic_tmp;
	delete[] data_mute;
	delete[] data_to_send;
	exit();
}


// EOF
