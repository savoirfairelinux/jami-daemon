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
#include <stdio.h>
#include <stdlib.h>
#include <qhostaddress.h>
#include <qstring.h>

#include "audiocodec.h"
#include "configuration.h"
#include "manager.h"
#include "audiortp.h"
#include "sip.h"
#include "../stund/stun.h"

#include <string>
using namespace ost;
using namespace std;


////////////////////////////////////////////////////////////////////////////////
// AudioRtp                                                          
////////////////////////////////////////////////////////////////////////////////
AudioRtp::AudioRtp (Manager *manager) {
	string svr;
	this->manager = manager;
	RTXThread = NULL;
	if (!manager->useStun()) {
		if (Config::gets("Signalisations", "SIP.sipproxy") == "") {
			svr = Config::gets("Signalisations", "SIP.sipproxy");
		}
	} else {
		svr = Config::gets("Signalisations", "SIP.hostPart");
	}
}

AudioRtp::~AudioRtp (void) {
}

int 
AudioRtp::createNewSession (SipCall *ca) {
	// Start RTP Send/Receive threads
	ca->enable_audio = 1;
	if (!manager->useStun()) { 
		symetric = false;
	} else {
		symetric = true;
	}
	
#ifdef ALSA
	if (manager->useAlsa) {
		RTXThread = new AudioRtpRTX (ca, manager->audiodriver, 
				manager->audiodriverReadAlsa, manager, symetric);
	}
#endif
	if (!manager->useAlsa) {
		RTXThread = new AudioRtpRTX (ca, manager->audiodriver, NULL, manager, 
			symetric);
	}
	
	if (RTXThread->start() != 0) {
		return -1;
	}
		
	return 0;
}

	
void
AudioRtp::closeRtpSession (SipCall *ca) {
	// This will make RTP threads finish.
	ca->enable_audio = -1;

	if (RTXThread != NULL) {
		// Wait for them...and delete.
		//RTXThread->join();
		qDebug("DELETED");
		delete RTXThread;
		RTXThread = NULL;
	}

	// Flush audio read buffer
	manager->audiodriver->resetDevice();
}

////////////////////////////////////////////////////////////////////////////////
// AudioRtpRTX Class                                                          //
////////////////////////////////////////////////////////////////////////////////
AudioRtpRTX::AudioRtpRTX (SipCall *sipcall, AudioDrivers *driver, 
		AudioDrivers *read_driver, Manager *mngr, bool sym) {
	this->manager = mngr;
	this->ca = sipcall;
	this->sym =sym;
	this->audioDevice = driver;
#ifdef ALSA
	if (manager->useAlsa) {
		this->audioDeviceRead = read_driver;
	}
#endif

	// TODO: Change bind address according to user settings.
	InetHostAddress local_ip("0.0.0.0");

	if (!sym) {
		sessionRecv = new RTPSession (local_ip, ca->getLocalAudioPort());
		sessionSend = new RTPSession (local_ip);
	} else {
		int forcedPort = manager->getFirewallPort();
		qDebug("Forced port %d", forcedPort);
		session = new SymmetricRTPSession (local_ip, forcedPort);
	}
	AudioCodec::gsmCreate();
}

AudioRtpRTX::~AudioRtpRTX () {
	this->terminate();
	AudioCodec::gsmDestroy();
	
	if (!sym) {
		if (sessionRecv != NULL) {
			delete sessionRecv;	
			sessionRecv = NULL;
		}
		if (sessionSend != NULL) {
			delete sessionSend;	
			sessionSend = NULL;
		}
	} else {
		if (session != NULL) {
			delete session;
			session = NULL;
		}
	}
}

void
AudioRtpRTX::run (void) {
//	AudioCodec 		 ac;
	unsigned char	*data_to_send;
	short			*data_mute;
	short			*data_from_mic;
	short			*data_from_mic_tmp;
	int				 i,
					 compSize, 
					 timestamp;
	int				 expandedSize;
	short			*data_for_speakers = NULL;
	int 			 countTime = 0;
	data_for_speakers = new short[2048];
	data_from_mic = new short[1024];
	data_from_mic_tmp = new short[1024];
	data_to_send = new unsigned char[1024];
	data_mute = new short[1024];

	InetHostAddress remote_ip(ca->remote_sdp_audio_ip);
	
	if (!remote_ip) {
	   qDebug("RTX: IP address is not correct!");
	   exit();
	} else {
		qDebug("RTX: Connected to %s:%d",
				ca->remote_sdp_audio_ip, ca->remote_sdp_audio_port);
	}
	
	// Initialization
	if (!sym) {
		sessionRecv->setSchedulingTimeout (100000);
		sessionRecv->setExpireTimeout(1000000);
		
		sessionSend->setSchedulingTimeout(10000);
		sessionSend->setExpireTimeout(1000000);
	} else {
		session->setSchedulingTimeout(10000);
		session->setExpireTimeout(1000000);
	}

#if 0 // Necessaire ?
    if (!sessionRecv->addDestination(remote_ip,
				(unsigned short) ca->remote_sdp_audio_port)) {
		qDebug("RTX recv: could not connect to port %d", 
				ca->remote_sdp_audio_port);
		this->exit();
	} else {
		qDebug("RTP(Recv): Added destination %s:%d",
				remote_ip.getHostname(),
				(unsigned short) ca->remote_sdp_audio_port);
	}
#endif

	if (!sym) {
		if (!sessionSend->addDestination (remote_ip, 
					(unsigned short) ca->remote_sdp_audio_port)) {
			qDebug("RTX send: could not connect to port %d", 
					ca->remote_sdp_audio_port);
			this->exit();
		} else {
			qDebug("RTP(Send): Added destination %s:%d",
					remote_ip.getHostname(),
					(unsigned short) ca->remote_sdp_audio_port);
		}

		sessionRecv->setPayloadFormat(StaticPayloadFormat(
				(StaticPayloadType) ca->payload));
		sessionSend->setPayloadFormat(StaticPayloadFormat(
				(StaticPayloadType) ca->payload));
		setCancel(cancelImmediate);
		sessionSend->setMark(true);

	} else {
		if (!session->addDestination (remote_ip, 
					(unsigned short) ca->remote_sdp_audio_port)) {
			qDebug("Symmetric: could not connect to port %d", 
					ca->remote_sdp_audio_port);
			this->exit();
		} else {
			qDebug("Symmetric: Connected to %s:%d",
					remote_ip.getHostname(),
					(unsigned short) ca->remote_sdp_audio_port);

			session->setPayloadFormat(StaticPayloadFormat(
				(StaticPayloadType) ca->payload));
			setCancel(cancelImmediate);
		}
	}
	
	timestamp = 0;

	// TODO: get frameSize from user config 
	int frameSize = 20; // 20ms frames
	TimerPort::setTimer(frameSize);
	
	// start running the packet queue scheduler.
	if (!sym) {
		sessionRecv->startRunning();
		sessionSend->startRunning();	
	} else {
		session->startRunning();
	}

	while (ca->enable_audio != -1) {
		////////////////////////////
		// Send session
		////////////////////////////
		//int size = AudioCodec::getSizeByPayload(ca->payload);
		int size = 320;
		if (!manager->mute) {
#ifdef ALSA
			if (manager->useAlsa) {
				i = audioDeviceRead->readBuffer (data_from_mic, size);
			}
#endif
			if (!manager->useAlsa) {
				i = audioDevice->readBuffer (data_from_mic, size);
			}
		} else {
			// When IP-phone user click on mute button, we read buffer of a
			// temp buffer to avoid delay in sound.
#ifdef ALSA
			if (manager->useAlsa)
				i = audioDeviceRead->readBuffer (data_mute, size);
#endif
			if (!manager->useAlsa)
				i = audioDevice->readBuffer (data_mute, size);
		}
	//qDebug("read i = %d", i); 
		// TODO : return an error because no sound
		if (i < 0) {
			break;
		}
		for (int j = 0; j < i; j++)
			data_from_mic_tmp[j] = data_from_mic[j]*manager->getMicVolume()/100;
		
		// Encode acquired audio sample
		compSize = AudioCodec::codecEncode (
				ca->payload, 
				data_to_send,
				data_from_mic_tmp, i);

		// Send encoded audio sample
		if (!sym) {
			sessionSend->putData(timestamp, data_to_send, compSize);
		} else {
			session->putData(timestamp, data_to_send, compSize);
		}
		//timestamp += compSize;
		timestamp += 160;

		////////////////////////////
		// Recv session
		////////////////////////////
		const AppDataUnit* adu = NULL;

		do {
			Thread::sleep(5); // in msec.
			if (!sym) {
				adu = sessionRecv->getData(sessionRecv->getFirstTimestamp());
			} else {
				adu = session->getData(session->getFirstTimestamp());
			}
		} while (adu == NULL);

		// Decode data with relevant codec
		expandedSize = AudioCodec::codecDecode (
				adu->getType(),
				data_for_speakers,
				(unsigned char*) adu->getData(),
				adu->getSize());
		
		// Set decoded data to sound device
		audioDevice->audio_buf.resize(expandedSize);
		audioDevice->audio_buf.setData (data_for_speakers, 
				manager->getSpkrVolume());

		// Notify (with a bip) an incoming call when there is already call 
		countTime += TimerPort::getElapsed();
		if (manager->sip->getNumberPendingCalls() != 1) {
			if ((countTime % 3000) <= 10 and (countTime % 3000) >= 0) {
				manager->notificationIncomingCall();
			}
		}
		
		// Write data or notification
		i = audioDevice->writeBuffer ();
		delete adu;


		// Let's wait for the next transmit cycle
		Thread::sleep(TimerPort::getTimer());
		TimerPort::incTimer(frameSize); // 'frameSize' ms
	}
		 
	delete[] data_for_speakers;
	delete[] data_from_mic;
	delete[] data_from_mic_tmp;
	delete[] data_mute;
	delete[] data_to_send;
	this->exit();
}


// EOF
