/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 *	Portions Copyright (C) 2002,2003   Aymeric Moizard <jack@atosc.org>
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

#include <vector>
#include <map> // for allowed
#include <eXosip2/eXosip.h>

#include "voIPLink.h"
#include "audio/audiortp.h"
#include "call.h" // for CALLID

#define EXPIRES_VALUE	180
// To build request
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
#define NOT_ACCEPTABLE_HERE 488 // Not Acceptable Here
// 5XX errors
#define SERVICE_UNAVAILABLE	503
// 6XX errors
#define	BUSY_EVERYWHERE	600
#define DECLINE			603

class AudioCodec;
class CodecDescriptor;
class SipCall;
class EventThread;

typedef std::vector< CodecDescriptor* > CodecDescriptorVector;
typedef std::list<std::string> AllowList;

class SipVoIPLink : public VoIPLink {
public:
  SipVoIPLink();
  virtual ~SipVoIPLink();
	
	virtual bool init (void);
	virtual bool checkNetwork (void);
	virtual void terminate (void);
	virtual int setRegister (void);
	virtual int setUnregister (void);
	virtual int outgoingInvite (CALLID id, const std::string& to_url);	
	virtual int answer (CALLID id);
	virtual int hangup (CALLID id);
	virtual int cancel (CALLID id);
	virtual int onhold (CALLID id);
	virtual int offhold (CALLID id);
	virtual int transfer (CALLID id, const std::string& to);
	virtual int refuse (CALLID id);	
	virtual int getEvent (void);
	virtual void carryingDTMFdigits (CALLID id, char code);
	
	/*
	 * To handle the local port
	 */
	int	getLocalPort (void);
	void setLocalPort (int);
  bool getSipLocalIp (void);

	/*
	 * Add a new SipCall at the end of the SipCallVector with identifiant 'id'
	 */
	void newOutgoingCall(CALLID callid);
	void newIncomingCall(CALLID callid);

	/*
	 * Erase the SipCall(id) from the SipCallVector
	 */
	void deleteSipCall(CALLID callid);
	
	/*
	 * Return a pointer to the SipCall with identifiant 'id'
	 */
	SipCall* getSipCall(CALLID callid);

	/*
	 * Accessor to the audio codec of SipCall with identifiant 'id'
	 */
	AudioCodec* getAudioCodec(CALLID callid);

// Handle voice-message
	inline void setMsgVoicemail (int nMsg) { _nMsgVoicemail = nMsg; }
	inline int getMsgVoicemail (void) { return _nMsgVoicemail; }

  /**
  * send text message
  * the size of message should not exceed 1300 bytes
  */
  virtual bool sendMessage(const std::string& to, const std::string& body);
	
private:
	/*
	 * If you are behind a NAT, you have to use STUN server, specified in 
	 * STUN configuration(you can change this one by default) to give you an 
	 * public IP address and assign a port number.
         * @param port : on which port we want to listen to
	 * 
	 * Return false if an error occured and true if no error.
	 */
	bool behindNat(int port);

	/*
	 * Return -1 if an error occured and 0 if no error
	 */
	int checkUrl(const std::string& url);

	/*
	 * Allow the authentication when you want register
   	 * Return -1 if an error occured and 0 if no error 
	 */	 
	int setAuthentication (void);

	/*
	 * Build a sip address from the user configuration
	 * Example: "Display user name" <sip:user@host.com>
	 * Return the result in a string
	 */
	std::string fromHeader (const std::string& user, const std::string& host);

	/*
	 * Build a sip address with the number that you want to call
	 * Example: sip:124@domain.com
	 * Return the result in a string
	 */
	std::string toHeader(const std::string& to);

	/*
	 * Beginning point to make outgoing call.
	 * Check the 'from' and 'to' url.
	 * Allocate local audio port.
	 * Build SDP body.
	 * Return -1 if an error occured and 0 if no error
	 */
	int startCall (CALLID id, const std::string& from, const std::string& to, const std::string& subject, const std::string& route);

  std::string getSipFrom();
  std::string getSipRoute();
  std::string getSipTo(const std::string& to_url);



	/*
	 * Look for call with same cid/did 
	 * Return the id of the found call
	 */
	CALLID findCallId (eXosip_event_t *e);
  CALLID findCallIdInitial (eXosip_event_t *e);

	/*
	 * To build sdp when call is on-hold
	 */
	int sdp_hold_call (sdp_message_t * sdp);
	
	/*
	 * To build sdp when call is off-hold
	 */
	int sdp_off_hold_call (sdp_message_t * sdp);

  /**
   * Subscribe to message summary
   */
	void subscribeMessageSummary();

  /**
   * End all sip call not deleted
   */
  void endSipCalls();

  /**
   * Handle DTMF Relay INFO Request
   */
  bool handleDtmfRelay(eXosip_event_t* event);

  /**
   * Get from a response event, all allowed request
   */
  bool setAllows(eXosip_event_t *event);

	///////////////////////////
	// Private member variables
	///////////////////////////
	EventThread 	*_evThread;
	std::vector< SipCall * > _sipcallVector;
  AllowList _allowList;

	AudioRtp 		_audiortp;
	int 			_localPort;
	int 			_reg_id;
	int 			_nMsgVoicemail;

  bool _registrationSend; // unregistered 
  bool _started; // eXosip_init and eXosip_start
};

#endif // __SIP_VOIP_LINK_H__
