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
#ifdef  CCXX_NAMESPACES
using namespace ost;
using namespace std;
#endif

////////////////////////////////////////////////////////////////////////////////
// AudioRtp                                                          
////////////////////////////////////////////////////////////////////////////////
AudioRtp::AudioRtp (SIP *sip, Manager *manager) {
	string svr;
	this->sip = sip;
	this->manager = manager;
	RTXThread = NULL;
#if 0	
	if (!manager->useStun()) {
		if (Config::gets("Signalisations/SIP.sipproxy")) {
			svr = Config::gets("Signalisations/SIP.sipproxy");
		}
	} else {
		svr = Config::gets("Signalisations/SIP.hostPart");
	}
#endif
	if (!manager->useStun()) {
		if (Config::gets("Signalisations", "SIP.sipproxy") == NULL) {
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

	RTXThread = new AudioRtpRTX (ca, manager->audiodriver, manager, symetric);
	RTXThread->start();
	
/*	if (!manager->useStun()) {
		RTXThread = new AudioRtpRTX (ca, manager->audiodriver, manager);
		qDebug("new RTXThread = 0x%X", (int)RTXThread);
		RTXThread->start();
	} else {
		symThread = new AudioRtpSymmetric (ca, manager->audiodriver, manager);
		symThread->start();
	}
*/	
	return 0;
}

	
void
AudioRtp::closeRtpSession (SipCall *ca) {
	// This will make RTP threads finish.
	ca->enable_audio = -1;

	if (RTXThread != NULL) {
	// Wait for them...and delete.
	RTXThread->join();
	
		delete RTXThread;
		qDebug ("RTXThread deleted!");
		RTXThread = NULL;
	}

/*	if (!manager->useStun()) {
		RTXThread->join();
		if (RTXThread != NULL) {
			delete RTXThread;
			qDebug ("RTXThread deleted!");
			RTXThread = NULL;
		}
	} else {
		symThread->join();
		if (symThread != NULL) {
			delete symThread;
			symThread = NULL;
		}
	}
*/	
	// Flush audio read buffer
	manager->audiodriver->resetDevice();
}

void
AudioRtp::rtpexit (void) {
}

////////////////////////////////////////////////////////////////////////////////
// AudioRtpRTX Class                                                          //
////////////////////////////////////////////////////////////////////////////////
AudioRtpRTX::AudioRtpRTX (SipCall *sipcall, AudioDrivers *driver, 
						Manager *mngr, bool sym) {
	this->ca = sipcall;
	this->audioDevice = driver;
	this->manager = mngr;
	this->sym =sym;

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
}

AudioRtpRTX::~AudioRtpRTX () {
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
	AudioCodec 		 ac;
	unsigned char	*data_to_send;
	short			*data_mute;
	short			*data_from_mic;
	int				 i,
					 compSize, 
					 timestamp;
	int				 expandedSize;
	short			*data_for_speakers = NULL;
	
	data_for_speakers = new short[2048];
	data_from_mic = new short[1024];
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
				(unsigned short) ca->remote_sdp_audio_port)
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
		if (!manager->mute) {
			i = audioDevice->readBuffer (data_from_mic, 320);
		} else {
			// When IP-phone user click on mute button, we read buffer of a
			// temp buffer to avoid delay in sound.
			i = audioDevice->readBuffer (data_mute, 320);
		}

		// Encode acquired audio sample
		compSize = AudioCodec::codecEncode (
				ac.handleCodecs[0], 
				data_to_send,
				data_from_mic, i);

		// Send encoded audio sample
		if (!sym) {
			sessionSend->putData(timestamp, data_to_send, compSize);
		} else {
			session->putData(timestamp, data_to_send, compSize);
		}
		timestamp += compSize;

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
		
		// Write decoded data to sound device
		i = audioDevice->writeBuffer (data_for_speakers, expandedSize);
		delete adu;


		// Let's wait for the next transmit cycle
		Thread::sleep(TimerPort::getTimer());
		TimerPort::incTimer(frameSize); // 'frameSize' ms
	}
		 
	delete[] data_for_speakers;
	delete[] data_from_mic;
	delete[] data_mute;
	delete[] data_to_send;
	this->exit();
}

#if 0
////////////////////////////////////////////////////////////////////////////////
// AudioRtpSymmetric Class                                                    //
////////////////////////////////////////////////////////////////////////////////
AudioRtpSymmetric::AudioRtpSymmetric (SipCall *sipcall, AudioDrivers *driver,
										Manager *mngr) {
	this->ca = sipcall;
	this->audioDevice = driver;
	this->manager = mngr;

	InetHostAddress local_ip("192.168.1.172");
	int forcedPort = manager->getFirewallPort();
	qDebug("port firewall = %d", forcedPort);

	session = new SymmetricRTPSession (local_ip, forcedPort);
}

AudioRtpSymmetric::~AudioRtpSymmetric () {
	delete session;	
	terminate();
}

void
AudioRtpSymmetric::run (void) {
	AudioCodec 		 ac;
	unsigned char	*data_to_send;
	short			*data_from_mic;
	int				 i,
					 compSize, 
					 timestamp;
	int				 expandedSize;
	short			*data_for_speakers = NULL;

	data_for_speakers = new short[2048];
	data_from_mic = new short[1024];
	data_to_send = new unsigned char[1024];

	InetHostAddress remote_ip;
	remote_ip = ca->remote_sdp_audio_ip;
	int remote_port = ca->remote_sdp_audio_port;
	
	if (!remote_ip) {
	   qDebug("Symmetric: IP address is not correct!");
	   exit();
	} 
	
	// Initialization
	session->setSchedulingTimeout(10000);
	session->setExpireTimeout(1000000);

	if (!session->addDestination (remote_ip, (unsigned short) remote_port)) {
		qDebug("Symmetric: could not connect to port %d", remote_port);
		this->exit();
	} else {
		qDebug("Symmetric: Connected to %s:%d",
				ca->remote_sdp_audio_ip, remote_port);
	}
	
    session->setPayloadFormat(StaticPayloadFormat(
				(enum StaticPayloadType) ca->payload));
	
	setCancel(cancelImmediate);
	
	timestamp = 0;

	// TODO: get frameSize from user config 
	int frameSize = 20; // 20ms frames
	TimerPort::setTimer(frameSize);
	
	// start running the packet queue scheduler.
	session->startRunning();	
 
	while (ca->enable_audio != -1) {
		////////////////////////////
		// Send session
		////////////////////////////
		i = audioDevice->readBuffer (data_from_mic, 320);
		// Encode acquired audio sample
		compSize = AudioCodec::codecEncode (
				ac.handleCodecs[0], 
				data_to_send,
				data_from_mic, i);

		// Send encoded audio sample
		session->putData(timestamp, data_to_send, compSize);
		timestamp += compSize;

		////////////////////////////
		// Recv session
		////////////////////////////
		const AppDataUnit* adu = NULL;
		
		do {
			Thread::sleep(10);
			adu = session->getData(session->getFirstTimestamp());	
		} while (adu == NULL);

		// Decode data with relevant codec
		expandedSize = AudioCodec::codecDecode (
				adu->getType(),
				data_for_speakers,
				(unsigned char*) adu->getData(),
				adu->getSize());

		// Write decoded data to sound device
		audioDevice->writeBuffer (data_for_speakers, expandedSize);
		delete adu;

		// Let's wait for the next cycle
		Thread::sleep(TimerPort::getTimer());
		TimerPort::incTimer(frameSize); // 'frameSize' ms
	}
		 
	delete[] data_for_speakers;
	delete[] data_from_mic;
	delete[] data_to_send;
	this->exit();
}
#endif

// EOF
