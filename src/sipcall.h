/*
 * Copyright (C) 2004 Savoir-Faire Linux inc.
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
#include <qapplication.h>

#include "manager.h"

#define NOT_USED      0

class SipCall {
public:
	SipCall (void);
	SipCall (Manager *);
	~SipCall (void);

	Manager *manager;
	bool 	 usehold;

	int 	 cid;	// call id
  	int 	 did;	// dialog id

  	int  	 status_code;
	
	char 	*reason_phrase;
  	char 	*textinfo;
  	char 	*req_uri;
  	char 	*local_uri;
  	char 	*remote_uri;
  	char 	*subject;
	
	char 	*remote_sdp_audio_ip;
  	char 	*payload_name;
	int		 local_audio_port;	
  	int  	 remote_sdp_audio_port;
 	int  	 payload;
  	int  	 state;
  	int  	 enable_audio; /* 1 started, -1 stopped */
	
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

private:
	void	alloc				(void);
	void	dealloc				(void);
		
};

#endif // __SIP_CALL_H__
