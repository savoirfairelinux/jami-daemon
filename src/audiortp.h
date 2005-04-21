/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
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

#include <ccrtp/rtp.h>
#include <cc++/numbers.h>

using namespace ost;

#include "sipcall.h"

#define LEN_BUFFER	160

class SIP;
class Manager;

///////////////////////////////////////////////////////////////////////////////
// Two pair of sockets
///////////////////////////////////////////////////////////////////////////////
class AudioRtpRTX : public Thread, public TimerPort {
public:
	AudioRtpRTX (SipCall *, AudioDrivers *, AudioDrivers *, Manager *, bool);
	~AudioRtpRTX();
	Time *time; 	// For incoming call notification 
	virtual void run ();

private:
	SipCall				*ca;
	AudioDrivers		*audioDevice;
#ifdef ALSA
	AudioDrivers		*audioDeviceRead;
#endif
	RTPSession 			*sessionSend;
	RTPSession 			*sessionRecv;
	SymmetricRTPSession	*session;
	Manager				*manager;
	bool				 sym;
};

///////////////////////////////////////////////////////////////////////////////
// Main class rtp
///////////////////////////////////////////////////////////////////////////////
class AudioRtp {
public:
	AudioRtp (Manager *);
	~AudioRtp (void);

	int 				createNewSession	(SipCall *);
	void				closeRtpSession		(SipCall *);

private:
	AudioRtpRTX			*RTXThread;
	SIP 				*sip;
	Manager				*manager;
	bool				 symetric;
};

#endif // __AUDIO_RTP_H__
