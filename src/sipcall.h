/*
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
 *
 * Portions Copyright (C) 2002,2003   Aymeric Moizard <jack@atosc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dpkg; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __SIP_CALL_H__
#define __SIP_CALL_H__

#include <eXosip/eXosip.h>
#include <vector>

#include "audio/audiocodec.h"
#include "audio/codecDescriptor.h"

#define NOT_USED      0
using namespace std;

typedef vector<CodecDescriptor*, allocator<CodecDescriptor*> > CodecDescriptorVector;

class AudioCodec;
class SipCall {
public:
	SipCall (short id, CodecDescriptorVector* cdv);
	~SipCall (void);

 	int  	payload;
  	int  	enable_audio; /* 1 started, -1 stopped */
	
	int  	newIncomingCall 	(eXosip_event_t *);
	int  	answeredCall 		(eXosip_event_t *);
//	int  	proceedingCall		(eXosip_event_t *);
	int  	ringingCall			(eXosip_event_t *);
	int  	redirectedCall		(eXosip_event_t *);
	int  	requestfailureCall	(eXosip_event_t *);
	int  	serverfailureCall	(eXosip_event_t *);
	int  	globalfailureCall	(eXosip_event_t *);
	
	int  	closedCall			(void);
	int  	onholdCall			(eXosip_event_t *);
	int  	offholdCall			(eXosip_event_t *);

	void	setLocalAudioPort 	(int);
	int 	getLocalAudioPort 	(void);	
	void 	setId				(short id);
	short	getId				(void);
	void 	setDid				(int did);
	int 	getDid				(void);
	void 	setCid				(int cid);
	int 	getCid				(void);
	int 	getRemoteSdpAudioPort (void);
	char* 	getRemoteSdpAudioIp (void);
	AudioCodec* getAudioCodec	(void);
	void	setAudioCodec		(AudioCodec* ac);

	inline void setStandBy (bool standby) { _standby = standby; }
	inline bool getStandBy (void) { return _standby; }

private:
	void	alloc			(void);
	void	dealloc			(void);
	void 	noSupportedCodec(void);	
	
	CodecDescriptorVector* _cdv;
	AudioCodec* _audiocodec;
	
	short 	_id;
	int 	_cid;	// call id
  	int 	_did;	// dialog id
	bool	_standby; // wait for a cid and did when outgoing call is made

  	int  	_status_code;
	
	char*	_reason_phrase;
  	char*	_textinfo;
  	char*	_req_uri;
  	char*	_local_uri;
  	char*	_remote_uri;
  	char*	_subject;
	
	char*	_remote_sdp_audio_ip;
  	char*	_payload_name;
	char*	_sdp_body;
  	int  	_state;
	int		_local_audio_port;
  	int  	_remote_sdp_audio_port;
	
};

#endif // __SIP_CALL_H__
