/**
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#include <eXosip2/eXosip.h>
#include <vector>
#include <string>
#include "call.h"

class CodecDescriptor;
class AudioCodec;

#define _SENDRECV 0
#define _SENDONLY 1
#define _RECVONLY 2

// Vector of CodecDescriptor
class SipCall {
public:
  SipCall(CALLID id, CodecDescriptorMap& codecMap);
  ~SipCall (void);

  /**
   * Store information about incoming call and negociate payload
   */
  int newIncomingCall(eXosip_event_t *);
  int newReinviteCall(eXosip_event_t *);
	
	/*
	 * Use to answer to a ONHOLD/OFFHOLD event 
	 */
	int answeredCall 		(eXosip_event_t *);
	
	/* 
	 * Use to answer to an incoming call 
	 */
	void  	answeredCall_without_hold (eXosip_event_t *);

	int  	ringingCall			(eXosip_event_t *);
	int  	receivedAck			(eXosip_event_t *);

	/*
	 * Manage local audio port for each sipcall
	 */
	void	setLocalAudioPort 	(int);
	int 	getLocalAudioPort 	(void);	

  std::string getLocalIp() { return _localIp; }
  void setLocalIp(const std::string& ip) { _localIp = ip; }

  bool isReinvite() { return _reinvite; }
  void endReinvite() { _reinvite = false; }; 

	/*
	 * Manage id, did (dialog-id), cid (call-id) and tid (transaction-id) 
	 * for each sipcall
	*/ 
	void 	setId				(CALLID id);
	CALLID	getId				(void);
	void 	setDid				(int did);
	int 	getDid				(void);
	void 	setCid				(int cid);
	int 	getCid				(void);
	void 	setTid				(int tid);
	int 	getTid				(void);

	/*
	 * Manage remote sdp audio port
	 */
	int 	getRemoteSdpAudioPort (void);
	char* 	getRemoteSdpAudioIp (void);

  /*
   * Manage audio codec
   */
  AudioCodec* getAudioCodec(void) { return _audiocodec; }
  void setAudioCodec(AudioCodec* ac) { _audiocodec = ac; }
  CodecDescriptorMap& getCodecMap() { return _codecMap; }

  /*
   * Accessor to remote-uri
   */
  inline char* getRemoteUri (void) { return _remote_uri; }

  /*
   * To avoid confusion when an incoming call occured in the same time 
   * that you make an outgoing call
   */
  inline void setStandBy (bool standby) { _standby = standby; }
  inline bool getStandBy (void) { return _standby; }

  std::string getName() const { return _name; }
  std::string getNumber() const { return _number; }
  int sdp_complete_message(sdp_message_t * remote_sdp, osip_message_t * msg);

private:
  void	alloc			(void);
  void	dealloc			(void);
  void 	noSupportedCodec(void);	

  int sdp_analyse_attribute (sdp_message_t * sdp, sdp_media_t * med);
	
  ///////////////////////////
  // Private member variables
  ///////////////////////////
  CodecDescriptorMap& _codecMap;
  AudioCodec* _audiocodec;
	
  CALLID 	_id;
  int 	_cid;		// call id
  int 	_did;		// dialog id
  int 	_tid;		// transaction id
  bool	_standby; 	// wait for a cid and did when outgoing call is made
  
  int  	_status_code;
  
  char*	_reason_phrase;
  char*	_textinfo;
  char*	_remote_uri;
  
  char*	_remote_sdp_audio_ip;
  int		_local_audio_port;
  int  	_remote_sdp_audio_port;
  int 	_local_sendrecv;           /* _SENDRECV, _SENDONLY, _RECVONLY */
  int 	_remote_sendrecv;          /* _SENDRECV, _SENDONLY, _RECVONLY */

  std::string _localIp;
  std::string _name;   // set by incoming call
  std::string _number; // set by incoming call

  bool _reinvite;
};

#endif // __SIP_CALL_H__
