/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#ifndef __SIP_VOIP_LINK_H__
#define __SIP_VOIP_LINK_H__

#include <eXosip2/eXosip.h>
#include <osipparser2/sdp_message.h>

#include <string>
#include <vector>

#include "voIPLink.h"

using namespace std;

#define EXPIRES_VALUE	180
#define INVITE_METHOD	"INVITE"
// 1XX responses
#define DIALOG_ESTABLISHED 101
#define RINGING			180
// 2XX
#define OK				200
// 4XX Errors
#define	BAD_REQ			400
#define	UNAUTHORIZED	401
#define	FORBIDDEN		403
#define NOT_FOUND		404
#define NOT_ALLOWED		405
#define NOT_ACCEPTABLE	406
#define AUTH_REQUIRED	407
#define REQ_TIMEOUT		408
#define UNSUP_MEDIA_TYPE	415
#define TEMP_UNAVAILABLE 480
#define	ADDR_INCOMPLETE	484
#define	BUSY_HERE		486
#define	REQ_TERMINATED	487
// 5XX errors
#define SERVICE_UNAVAILABLE	503
// 6XX errors
#define	BUSY_EVERYWHERE	600
#define DECLINE			603

class AudioCodec;
class AudioRtp;
class CodecDescriptor;
class EventThread;
class SipCall;

typedef vector<SipCall*, allocator<SipCall*> > SipCallVector;
typedef vector<CodecDescriptor*, allocator<CodecDescriptor*> > CodecDescriptorVector;

class SipVoIPLink : public VoIPLink {
public:
	SipVoIPLink (short id);
	virtual ~SipVoIPLink (void);
	
	virtual int init (void);
	virtual bool checkNetwork (void);
	virtual void initRtpmapCodec (void);
	virtual void quit (void);
	virtual int setRegister (void);
	virtual int outgoingInvite (short id, const string& to_url);	
	virtual int answer (short id);
	virtual int hangup (short id);
	virtual int cancel (short id);
	virtual int onhold (short id);
	virtual int offhold (short id);
	virtual int transfer (short id, const string& to);
	virtual int refuse (short id);	
	virtual int getEvent (void);
	virtual void carryingDTMFdigits (short id, char code);
	
	int	getLocalPort (void);
	void setLocalPort (int);

	/*
	 * Add a new SipCall at the end of the SipCallVector with identifiant 'id'
	 */
	void newOutgoingCall(short callid);
	void newIncomingCall(short callid);

	/*
	 * Erase the SipCall(id) from the SipCallVector
	 */
	void deleteSipCall(short callid);
	
	/*
	 * Return a pointer to the SipCall with identifiant 'id'
	 */
	SipCall* getSipCall(short callid);

	AudioCodec* getAudioCodec(short callid);

	// To Cancel
	inline void setCid (int cid) { _cid = cid; }
	inline int getCid (void) { return _cid; }
	
private:
	int behindNat (void);
	int getLocalIp (void);
	int checkUrl(const string& url);
	int setAuthentication (void);
	string fromHeader (const string& user, const string& host);
	string toHeader(const string& to);
	int startCall (short id, const string& from, const string& to, 
			const string& subject, const string& route);
	/*
	 * Look for call with same cid/did 
	 * Return the id of the found call
	 */
	short findCallId (eXosip_event_t *e);
	short findCallIdWhenRinging (void);
	bool isInRtpmap (int index, int payload, CodecDescriptorVector* cdv);

	/*
	 * To build sdp with 200 OK when answered call
	 */
	int sdp_complete_200ok(int did, osip_message_t * answer, int port);
	
	/*
	 * To build sdp when call is on-hold
	 */
	int sdp_hold_call (sdp_message_t * sdp);
	
	/*
	 * To build sdp when call is off-hold
	 */
	int sdp_off_hold_call (sdp_message_t * sdp);
	
	EventThread* 	_evThread;
	SipCallVector* 	_sipcallVector;
	AudioRtp* 		_audiortp;
	int 			_localPort;
	int 			_cid;
	
};

#endif // __SIP_VOIP_LINK_H__
