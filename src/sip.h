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

#ifndef __SIP_H__
#define __SIP_H__

#include <sys/time.h>

#include <eXosip/eXosip.h>
#include <osip2/osip.h>
#include <osipparser2/osip_const.h>
#include <osipparser2/osip_headers.h>
#include <osipparser2/osip_body.h>

#include <qthread.h>

#include "audiocodec.h"
#include "manager.h"
#include "phoneline.h"
#include "sipcall.h"

// List of actions
#define ANSWER_CALL		0
#define	CLOSE_CALL		1
#define	ONHOLD_CALL		2
#define	OFFHOLD_CALL	3
#define TRANSFER_CALL	4
#define CANCEL_CALL		5

// 4XX Errors
#define	FORBIDDEN		403
#define NOT_FOUND		404
#define AUTH_REQUIRED	407
#define	ADDR_INCOMPLETE	484

class EventThread : public QThread {
public:
	EventThread (SIP *);
	virtual void 	 run ();
private:
	SIP	*	sipthread;
};


class SIP {
public:
	SIP		(void) {}
	SIP 	(Manager*);
	~SIP 	(void);

	int  	getNumberPendingCalls	(void);
	int 	findLineNumberNotUsed	(void);
	int		findLineNumber 			(eXosip_event_t *);
	int		findLineNumberUsed 		(eXosip_event_t *);
	int		findLineNumberClosed 	(eXosip_event_t *);	
	SipCall *findCall				(int pos);
	int 	initSIP 				(void);	
	void	quitSIP					(void);
	void 	initRtpmapCodec			(void);
	int 	checkURI				(const char *);
	int 	startCall 				(char *,  char *,  char *,  char *);
	int 	setRegister 			(void);
	int 	setAuthentication		(void);
	QString	fromHeader				(QString, QString, QString);
	QString toHeader				(QString);
	int 	outgoingInvite			(void);
	int 	manageActions 			(int, int);
	int 	getEvent				(void);
	void 	carryingDTMFdigits		(int, char);
	int		getLocalPort			(void);
	void	setLocalPort			(int);
	
	static int checkUrl				(char *url);
	
	char			*myIPAddress;
	SipCall 		*call[NUMBER_OF_LINES];
	Manager			*callmanager;
	

	int notUsedLine;
	
private:
	EventThread	*evThread;
	
	int 	getLocalIp 				(void);
	bool	isInRtpmap 				(int, int, AudioCodec&);
	int 	local_port;	
};

#endif // __SIP_H__
