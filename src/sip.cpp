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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <linux/socket.h>

#include <qlineedit.h>
#include <qcheckbox.h>

#include "audiocodec.h"
#include "configuration.h"
#include "global.h"
#include "sip.h"
#include "sipcall.h"

#include <string>
using namespace std;

// TODO : mettre dans config
#define DEFAULT_SIP_PORT	5060
#define RANDOM_SIP_PORT		rand() % 64000 + 1024
#define	DEFAULT_LOCAL_PORT	10500
#define	RANDOM_LOCAL_PORT	((rand() % 27250) + 5250)*2


///////////////////////////////////////////////////////////////////////////////
// Thread implementation 
///////////////////////////////////////////////////////////////////////////////
EventThread::EventThread (SIP *sip) : Thread () {
	this->sipthread = sip;
}

EventThread::~EventThread (void) {
	this->terminate();
}

/**
 * Reimplementation of run() to update widget
 */
void
EventThread::run (void) {
	for (;;) {
		sipthread->getEvent();
	}
}

///////////////////////////////////////////////////////////////////////////////
// SIP implementation
///////////////////////////////////////////////////////////////////////////////

SIP::SIP (Manager *_manager) {
	this->callmanager = _manager;
	this->myIPAddress = NULL;
	
	// For EventThread
	evThread = new EventThread (this);

	// Call init to NULL 
	for (int i = 0; i < NUMBER_OF_LINES; i++) {
		assert (i < NUMBER_OF_LINES);
		this->call[i] = NULL;
	}
	notUsedLine = -1;
}

SIP::~SIP (void) {
	if (evThread != NULL) {
		delete evThread;
		evThread = NULL;
	}
	delete[] call;
	delete myIPAddress;
}

// Init eXosip and set user agent 
int
SIP::initSIP (void) {
	QString tmp;

	tmp = QString(PROGNAME) + "/" + QString(VERSION);

	// Set IP address
	if ( getLocalIp() == -1 )
		return -1;
	
	srand (time(NULL));
	if (eXosip_init (NULL, NULL, DEFAULT_SIP_PORT) != 0) {
		if (eXosip_init (NULL, NULL, RANDOM_SIP_PORT) != 0) {
			qDebug("Cannot init eXosip");
			return -1;
		}
	}

	// If use STUN server, firewall address setup
	if (callmanager->useStun()) {
		eXosip_set_user_agent(tmp.ascii());
		StunAddress4 stunSvrAddr;
		stunSvrAddr.addr = 0;
		
		// Stun server
		string svr = Config::gets("Signalisations", "STUN.STUNserver");
		qDebug("address server stun = %s", svr.data());
		
		// Convert char* to StunAddress4 structure
		bool ret = stunParseServerName ((char*)svr.data(), stunSvrAddr);
		if (!ret) {
			qDebug("SIP: Stun server address not valid");
		}
		
		// Firewall address
		callmanager->getInfoStun(stunSvrAddr);
		eXosip_set_firewallip((callmanager->getFirewallAddress()).ascii());
	} 
	eXosip_set_user_agent(tmp.ascii());

	evThread->start();
	return 0;
}

void
SIP::quitSIP (void) {
	eXosip_quit();
}

bool
SIP::isInRtpmap (int index, int payload, AudioCodec &codec) {
	for (int i = 0; i < index; i++) {
		if (codec.handleCodecs[i] == payload) {
			return true;
		}
	}
	return false;
}

void
SIP::initRtpmapCodec (void) {
	int payload;
	char rtpmap[128];
	char tmp[64];

	bzero(rtpmap, 128);
	bzero(tmp, 64);
	
	AudioCodec codec;
 	/* reset all payload to fit application capabilities */
  	eXosip_sdp_negotiation_remove_audio_payloads();
	
	// Set rtpmap according to the supported codec order
	for (int i = 0; i < NB_CODECS; i++) {
		payload = codec.handleCodecs[i];

		// Add payload to rtpmap if it is not already added
		if (!isInRtpmap(i, payload, codec)) {
			snprintf(rtpmap, 127, "%d %s/%d", payload, 
					codec.rtpmapPayload(payload), SAMPLING_RATE);
			snprintf(tmp, 63, "%d", payload);
			
			eXosip_sdp_negotiation_add_codec( osip_strdup(tmp), NULL,
					   osip_strdup("RTP/AVP"), NULL, NULL, NULL, NULL,NULL,
					   osip_strdup(rtpmap));
		}
	}
#if 0    
  	eXosip_sdp_negotiation_add_codec(osip_strdup("111"),
				   NULL,
				   osip_strdup("RTP/AVP"),
				   NULL, NULL, NULL,
				   NULL,NULL,
				   osip_strdup("111 speex/16000"));

  /* Those attributes should be added for speex
     b=AS:110 20
     b=AS:111 20
  */
#endif
}

int 
SIP::getLocalIp (void) {
	if (myIPAddress == NULL) {
		myIPAddress = new char[64];
	}
	int ret = eXosip_guess_localip (2, myIPAddress, 64);
	return ret;
}

//Number of lines used
int
SIP::getNumberPendingCalls(void) {
  	int pos = 0;
  	int k;
  
  	for (k = 0; k < NUMBER_OF_LINES; k++) {
		assert (k < NUMBER_OF_LINES);
	    if (call[k]->state != NOT_USED) {
	  		pos++;
		}
    }
  	return pos;
}

// Return the first number of line not used
int
SIP::findLineNumberNotUsed (void) {
	int k;
	
	for (k = 0; k < NUMBER_OF_LINES; k++) {
		assert (k < NUMBER_OF_LINES);
		if (call[k] == NULL) {
			return k;
		}
    }

	// Every line is busy
	// TODO: ATTENTION AU RETOUR
	return -1;
}

// For ringing, answered, proceeding call
int
SIP::findLineNumber (eXosip_event_t *e) {
	int k;
	
	// Look for call with same cid/did
	for (k = 0; k < NUMBER_OF_LINES; k++) {
		if (call[k] != NULL) {
			if (call[k]->cid == e->cid and call[k]->did == e->did) {
				return k;
			}
		}
    }

	// If not found, return free line ?
	return findLineNumberNotUsed();
}

// For redirected, request,server,global failure, onhold, offhold call
int
SIP::findLineNumberUsed (eXosip_event_t *e) {
	return findLineNumber (e);
}

int
SIP::findLineNumberClosed (eXosip_event_t *e) {
	for (int k = 0; k < NUMBER_OF_LINES; k++) {
		if (call[k] != NULL) {
			if (call[k]->cid == e->cid) {
				return k;
			}
		} 
    }

	// TODO ATTENTION AU RETOUR
	return -1;
}
  
// Return call corresponding to pos
SipCall*
SIP::findCall (int pos) {
	for (int k = 0; k < NUMBER_OF_LINES; k++) {
		if (call[k] != NULL) {
			if (pos == 0) {
				return call[k];
			}
			pos--;
		}
	}
	return NULL;
}

int
SIP::checkURI (const char *buffer) {
	osip_uri_t *uri;
	int 		i;

	// To parse a buffer containing a sip URI
	i = osip_uri_init(&uri);
	if (i != 0) {
		qWarning ("Cannot allocate");
	   	return -1; 
	}
	i = osip_uri_parse(uri, buffer);
	if (i != 0) { 
		qWarning("Cannot parse uri"); 
	}

	// Free memory
	osip_uri_free(uri);	
	return 0;	
}

// Parse url
int
SIP::checkUrl(char *url) {
  	int i;
	
  	osip_from_t *to;
  	i = osip_from_init(&to);
  	if (i != 0) {
		qWarning ("Cannot initialize");
		return -1;
	}
  	i = osip_from_parse(to, url);
  	if (i != 0) {
		qWarning ("Cannot parse url");
		return -1;
	}

	// Free memory
	osip_from_free (to);
  	return 0;
}

int
SIP::setRegister (void) {
	int reg_id = -1;

	string qproxy = "sip:" + Config::gets("Signalisations", "SIP.sipproxy"); 
	char * proxy = (char*)qproxy.data();

	string qhostname = "sip:" + Config::gets("Signalisations", "SIP.hostPart"); 
	char * hostname = (char*)qhostname.data();
	
	string qfrom = fromHeader(Config::gets("Signalisations", "SIP.userPart"), 
							Config::gets("Signalisations", "SIP.hostPart"));
	char * from = (char*)qfrom.data();
	
	qDebug("proxy = %s", proxy);
	qDebug("from = %s", from);

	if (Config::gets("Signalisations", "SIP.userPart") == "") {
		callmanager->errorDisplay("Fill user part field");
		return -1;
	} 
	if (Config::gets("Signalisations", "SIP.hostPart") == "") {
		callmanager->errorDisplay("Fill host part field");		
		return -1;
	}
	
	eXosip_lock();
	if (setAuthentication() == -1) {
		return -1;
	}
	
	if (Config::gets("Signalisations", "SIP.sipproxy") != "") {
		reg_id = eXosip_register_init(from, proxy, NULL);
	} else {
		reg_id = eXosip_register_init(from, hostname, NULL);
	}
	
	if (reg_id < 0) {
		eXosip_unlock();
		return -1;
	}	
	
	// TODO: port SIP session timer dans config
	int i = eXosip_register(reg_id, 3600);
	if (i == -2) {
		qDebug("cannot build registration, check the setup");
	}
	if (i == -1) {
		qDebug("Registration Failed");
	}
	
	eXosip_unlock();
	return 0;
}

int
SIP::setAuthentication (void) {
	string login, pass, realm;
	login = Config::gets("Signalisations", "SIP.username");
	if (login == "") {
		login = Config::gets("Signalisations", "SIP.userPart");
	}
	pass = Config::gets("Signalisations", "SIP.password");
	if (pass == "") {
		callmanager->errorDisplay("Fill password field");				
		return -1;
	}

	if (callmanager->useStun()) {
		realm = Config::gets("Signalisations", "SIP.hostPart");
	} else {
		if (Config::gets("Signalisations", "SIP.sipproxy") != "") {
			realm = Config::gets("Signalisations", "SIP.sipproxy");
		} else {
			realm = Config::gets("Signalisations", "SIP.hostPart");
		}
	}
	
	if (eXosip_add_authentication_info(login.data(), login.data(), 
		pass.data(), NULL, NULL) != 0) {
		qDebug ("No authentication");
		return -1;
	}
	return 0;
}
		
string
SIP::fromHeader (string user, string host) {
	string displayname = Config::gets("Signalisations", "SIP.fullName");
	return ("\"" + displayname + "\"" + " <sip:" + user + "@" + host + ">");
}


string
SIP::toHeader(string to) {
	if (to.find("sip:") == string::npos) {
		return ("sip:" + to );
	} else {
		return to;
	}
}

int
SIP::startCall ( char *from,  char *to,  char *subject,  char *route) {
  	osip_message_t *invite;
  	int i;

  	if (checkUrl(from) != 0) {
		callmanager->errorDisplay("Error for From header");
    	return -1;
  	}
  	if (checkUrl(to) != 0) {
		callmanager->errorDisplay("Error for To header");
    	return -1;
  	}
  	
	i = eXosip_build_initial_invite(&invite, to, from, route, subject);	
  	if (i != 0) {
		return -1;
    }
  	
	eXosip_lock();
	if (!callmanager->useStun()) {
		char port[64];

		// Set random port for outgoing call
		setLocalPort(RANDOM_LOCAL_PORT);

		bzero (port, 64);
		snprintf (port, 63, "%d", getLocalPort());
		
  		i = eXosip_initiate_call (invite, NULL, NULL, port);
		
	} else {
		QString qport;
		qport = qport.number(callmanager->getFirewallPort());
		
		i = eXosip_initiate_call(invite, NULL, NULL, (char*)qport.ascii());

		qDebug("sip invite: port = %s",(char*)qport.ascii());
	}
	if (i <= 0) {
		qDebug("NO initiate_call = %d", i);
		eXosip_unlock();
		return -1;
	}

	eXosip_unlock();
  	return i;	
}

int
SIP::outgoingInvite (void) {
	char * from;
	char * to;

	// Form the From header field basis on configuration panel
	string qfrom = fromHeader(Config::gets("Signalisations", "SIP.userPart"),
								Config::gets("Signalisations", "SIP.hostPart"));
	from = (char*)qfrom.data();
	
	// Form the To header field
	string qto;
	if (callmanager->bufferTextRender().ascii() == NULL) 
		return -1;
	else 
		qto = toHeader(string(callmanager->bufferTextRender().ascii()));

	if (qto.find("@") == string::npos and 
			Config::getb("Signalisations", "SIP.autoregister")) {
		qto = qto + "@" + Config::gets("Signalisations", "SIP.hostPart");
	}
	to = (char*)qto.data();
		
	qDebug ("From: %s", from);
	qDebug ("To: <%s>", to);

	// If no SIP proxy setting
	if (Config::gets("Signalisations", "SIP.sipproxy") == "") {
		if (startCall(from, to, NULL, NULL) <= 0) {
			qDebug("SIP: no start call");
			return -1;
		}
		return 0;
	}
	
	string qroute = "<sip:" + Config::gets("Signalisations", "SIP.sipproxy") 
							+ ";lr>";
	char * route = (char*)qroute.data();
	
	if (startCall(from, to, NULL, route) <= 0) {
		qDebug("SIP: no start call");
		return -1;
	}
	return 0;
}

int
SIP::getLocalPort (void) {
	return local_port;
}

void
SIP::setLocalPort (int port) {
	this->local_port = port;
}
	
void
SIP::carryingDTMFdigits (int line, char digit) {
	  int duration = Config::geti("Signalisations", "DTMF.pulseLength"); 
      
	  static const int body_len = 128;

      char *dtmf_body = new char[body_len];
	  snprintf(dtmf_body, body_len - 1,
			  "Signal=%c\r\nDuration=%d\r\n",
			  digit, duration);

      eXosip_lock();
      eXosip_info_call(call[line]->did, "application/dtmf-relay", dtmf_body);
      eXosip_unlock();
	
	  delete[] dtmf_body;
}

// Handle IP-Phone user actions
int
SIP::manageActions (int usedLine, int action) {
	int i;
	string referTo;
	
	char tmpbuf[64];

	assert (usedLine < NUMBER_OF_LINES);
	assert (usedLine >= 0);
	
	bzero (tmpbuf, 64);

	switch (action) {
		
	// IP-Phone user is answering a call.
	case ANSWER_CALL: 
		qDebug("ANSWER_CALL line %d, cid = %d, did = %d", 
				usedLine, call[usedLine]->cid, call[usedLine]->did);

		//callmanager->setCallInProgress(false);
		callmanager->phLines[usedLine]->setbInProgress(false);
		
		// Get local port
		snprintf (tmpbuf, 63, "%d", call[usedLine]->getLocalAudioPort());

		eXosip_lock();
		i = eXosip_answer_call(call[usedLine]->did, 200, tmpbuf);
		eXosip_unlock();
			
		// Incoming call is answered, start the sound channel.
		if (callmanager->startSound (call[usedLine]) < 0) {
			qDebug ("FATAL: Unable to start sound (%s:%d)",
					__FILE__, __LINE__);
			exit(1);
		}
		break;

	// IP-Phone user is hanging up.
	case CLOSE_CALL: // 1
		qDebug("CLOSE_CALL: cid = %d et did = %d", call[usedLine]->cid, 
				call[usedLine]->did);
		
		call[usedLine]->usehold = false;
		// Release SIP stack.
		eXosip_lock();
		i = eXosip_terminate_call (call[usedLine]->cid, call[usedLine]->did);
		eXosip_unlock();
		qDebug ("exosip_terminate_call = %d", i);

		// Release RTP channels
		call[usedLine]->closedCall();

		// Delete the call when I hangup
		if (call[usedLine] != NULL) {
			qDebug("SIP: CLOSE_CALL delete call[%d]", usedLine);
			delete call[usedLine];
			call[usedLine] = NULL;
		}
		break;

	// IP-Phone user is parking peer on HOLD
	case ONHOLD_CALL:
		call[usedLine]->usehold = true;
		
		eXosip_lock();
		i = eXosip_on_hold_call(call[usedLine]->did);
		eXosip_unlock();
		
		// Disable audio
		call[usedLine]->closedCall();
		break;

	// IP-Phone user is parking peer OFF HOLD
	case OFFHOLD_CALL:
		call[usedLine]->usehold = true;
		eXosip_lock();
		i = eXosip_off_hold_call(call[usedLine]->did, NULL, 0);
		eXosip_unlock();

		// Enable audio
		if (callmanager->startSound (call[usedLine]) < 0) {
			qDebug ("FATAL: Unable to start sound (%s:%d)",
					__FILE__, __LINE__);
			exit(1);
		}
		break;

	// IP-Phone user is transfering call
	case TRANSFER_CALL:
		referTo	= toHeader(string(callmanager->bufferTextRender().ascii()));
		if (referTo.find("@") == string::npos) {
			referTo = referTo + "@" + 
				Config::gets("Signalisations", "SIP.hostPart");
		}
		
		eXosip_lock();
		i = eXosip_transfer_call(call[usedLine]->did, (char*)referTo.data());
		eXosip_unlock();
		break;
		
	// IP-Phone user is hanging up during ringing
	case CANCEL_CALL:
		qDebug("SIP: CANCEL_CALL: terminate cid=%d did=%d", 
				call[usedLine]->cid,call[usedLine]->did);
		eXosip_lock();
		i = eXosip_terminate_call (call[usedLine]->cid, call[usedLine]->did);
		eXosip_unlock();

		callmanager->ringTone(false);
		
		// Delete the call when I hangup
		if (call[usedLine] != NULL) {
			delete call[usedLine];
			call[usedLine] = NULL;
		}
		break;

	default:
		return -1;
		break;
	}
	return 0;
}


// Receive events from the other
int
SIP::getEvent (void) {
	eXosip_event_t *event;
	int theline = -1;
	int curLine;
	char *name;
	static int countReg = 0;

	event = eXosip_event_wait (0, 50);
	if (event == NULL) {
		return -1;
	}	
	
	callmanager->handleRemoteEvent(event->status_code,
			event->reason_phrase,
			-1);	
	switch (event->type) {
		// IP-Phone user receives a new call
		case EXOSIP_CALL_NEW: //13
			qDebug("<- (%i %i) INVITE from: %s", event->cid, event->did,
				event->remote_uri);

			// Set local random port for incoming call
			setLocalPort(RANDOM_LOCAL_PORT);
			
			theline = findLineNumberNotUsed();
			notUsedLine = theline;

			if (theline < 0) {
				// TODO: remonter erreur au manager (on refuse l'appel)
			}
			assert (theline >= 0); assert (theline < NUMBER_OF_LINES);

			// Display the name which the call comes from
			osip_from_t *from;
  			osip_from_init(&from);
  			osip_from_parse(from, event->remote_uri);
			name = osip_from_get_displayname(from);
			callmanager->nameDisplay(name);
			callmanager->phLines[theline]->text = QString(name);
			osip_from_free(from);
			
			if (call[theline] == NULL) {
				//callmanager->setCallInProgress(true);
				callmanager->phLines[theline]->setbInProgress(true);
				call[theline] = new SipCall (callmanager);
				call[theline]->newIncomingCall(event);

				// Associate an audio port with a call
				call[theline]->setLocalAudioPort(local_port);

				// Ringing starts when somebody calls IP-phone user
				callmanager->ring();
				callmanager->phLines[theline]->setbRinging(true);
			}
			break;

		// The callee answers
		case EXOSIP_CALL_ANSWERED: // 8, start call: event->status_code = 200
			qDebug("ANSWERED<- (%i %i) [%i %s] %s", event->cid, event->did, 
				event->status_code, event->reason_phrase,
				event->remote_uri);
			   	
			// TODO: stop the ringtone
			callmanager->ringTone(false);
			
			curLine = callmanager->getCurrentLineNumber();
			theline = findLineNumber(event);
			if (call[theline] == NULL) {
				call[theline] = new SipCall(callmanager);
			}

			// Stop the call progress
			//callmanager->setCallInProgress(false);
			callmanager->phLines[theline]->setbInProgress(false);
			
			// Conditions to not start sound when callee answers for onhold
			// and offhold state
			if (callmanager->phLines[theline]->getStateLine() == BUSY or 
					callmanager->phLines[theline]->getStateLine() == OFFHOLD
					or !call[theline]->usehold) {
				
				if (!callmanager->transferedCall()) {
					if (!call[theline]->usehold) {
						// Associate an audio port with a call
						call[theline]->setLocalAudioPort(local_port);
					}

					// Answer
					call[theline]->answeredCall(event);

					// Handle event
					callmanager->handleRemoteEvent (
							0, NULL, EXOSIP_CALL_ANSWERED);

					callmanager->setChoose (false, false);
					
					// Outgoing call is answered, start the sound channel.
					if (callmanager->startSound (call[theline]) < 0) {
						qDebug ("FATAL: Unable to start sound (%s:%d)",
							__FILE__, __LINE__);
						exit(1);
					}
				}
			} else if (callmanager->otherLine() and
					call[curLine] == NULL) {
				// If a new line replaces the used current line and 
				// it's not ringing.
				callmanager->startDialTone();	
			}
			break;

		case EXOSIP_CALL_RINGING:
			qDebug("RINGING<- (%i %i) [%i %s] %s", event->cid, event->did, 
				event->status_code, event->reason_phrase, 
				event->remote_uri);
			
			callmanager->handleRemoteEvent (0, NULL, EXOSIP_CALL_RINGING);
			// TODO : rajouter tonalite qd ca sonne
			//callmanager->ringTone(true);

			// If we have chosen the line before validating the phone number
			if (callmanager->isChosenLine() and !callmanager->otherLine()) {
				theline = callmanager->chosenLine();
			} else {
				theline = findLineNumber(event);
			}

			if (theline < 0) {
				// TODO: remonter erreur au manager (on refuse l'appel)
			}
			assert (theline >= 0);
			assert (theline < NUMBER_OF_LINES);

			if (call[theline] == NULL) {
				call[theline] = new SipCall(callmanager);
				call[theline]->ringingCall(event);
			}
			break;

		case EXOSIP_CALL_REDIRECTED:
			qDebug("REDIRECTED<- (%i %i) [%i %s] %s",event->cid, event->did,
				event->status_code, event->reason_phrase, 
				event->remote_uri);
			// theline = findLineNumberUsed(event);
			//assert (theline >= 0);
			//assert (theline < NUMBER_OF_LINES);
			//? call[theline] = new SipCall(callmanager);
			// call[theline]->redirectedCall(event);
			break;

		case EXOSIP_CALL_REQUESTFAILURE:
			qDebug("REQUESTFAILURE<- (%i %i) [%i %s] %s", event->cid, 
				event->did, event->status_code, event->reason_phrase,
				event->remote_uri);
				
				theline = findLineNumber(event);
				assert (theline >= 0);
				assert (theline < NUMBER_OF_LINES);
			//	call[theline]->requestfailureCall(event);
				callmanager->phLines[theline]->setbInProgress(false);

			// Handle 4XX errors
			switch (event->status_code) {
				case AUTH_REQUIRED:
					eXosip_lock();
					if (setAuthentication() == -1) {
						break;
					}
					eXosip_retry_call (event->cid);
					eXosip_unlock();
					break;
				case NOT_FOUND:
				//	callmanager->setCallInProgress(false);
					callmanager->congestion(true);
					break;
				case ADDR_INCOMPLETE:
				//	callmanager->setCallInProgress(false);
					callmanager->congestion(true);
					break;
				case FORBIDDEN:
					callmanager->congestion(true);
					break;
				case REQ_TERMINATED:
					break;
				default:
					//callmanager->setCallInProgress(false);
					break;
			}
			break; 

		case EXOSIP_CALL_SERVERFAILURE:
			qDebug("SERVERFAILURE<- (%i %i) [%i %s] %s", event->cid, 
				event->did, event->status_code, event->reason_phrase,
				event->remote_uri);
			// Handle 5XX errors
			switch (event->status_code) {
				case SERVICE_UNAVAILABLE:
					callmanager->ringTone(false);
					callmanager->congestion(true);					
					break;
				default:
					break;
			}
			//theline = findLineNumberUsed(event);
			//assert (theline >= 0);
			//assert (theline < NUMBER_OF_LINES);
		//	call[theline]->serverfailureCall(event);
			break;

		case EXOSIP_CALL_GLOBALFAILURE:
			qDebug("GLOBALFAILURE<- (%i %i) [%i %s] %s", event->cid, 
				event->did, event->status_code, event->reason_phrase,
				event->remote_uri);
			// Handle 6XX errors
			
		//	call[findLineNumberUsed(event)]->globalfailureCall(event);
			break;

		// The remote peer closed the phone call(we received BYE).
		case EXOSIP_CALL_CLOSED:
			qDebug("<- (%i %i) BYE from: %s", event->cid, event->did, 
				event->remote_uri);

			call[theline]->usehold = false;
			theline = findLineNumber(event);
			if (!callmanager->phLines[theline]->getbInProgress()) {
		//	if (!callmanager->getCallInProgress()) {
				// If callee answered before closing call 
				// or callee not onhold by caller
			 	theline = findLineNumberClosed(event);
				call[theline]->closedCall();
			} else {
				// If callee closes call instead of answering
				theline = notUsedLine;
				// If call refused, stop ringTone
				callmanager->ringTone(false);
			}
			
			assert (theline >= 0);
			assert (theline < NUMBER_OF_LINES);

			if (call[theline] != NULL) {
				delete call[theline];
				call[theline] = NULL;
				callmanager->handleRemoteEvent(
						0, NULL, EXOSIP_CALL_CLOSED, theline);
			}
			break;

		case EXOSIP_CALL_HOLD:
			qDebug("<- (%i %i) INVITE (On Hold) from: %s", event->cid, 
				event->did, event->remote_uri);
			theline = findLineNumber(event);
			call[theline]->closedCall();
			call[theline]->onholdCall(event);
			break;

		case EXOSIP_CALL_OFFHOLD:
			qDebug("<- (%i %i) INVITE (Off Hold) from: %s", event->cid, 
				event->did, event->remote_uri);
			theline = findLineNumber(event);
			if (callmanager->startSound (call[theline]) < 0) {
				qDebug ("FATAL: Unable to start sound (%s:%d)",
						__FILE__, __LINE__);
				exit(1);
			}
			call[theline]->offholdCall(event);
			break;

		case EXOSIP_REGISTRATION_SUCCESS:
			qDebug("REGISTRATION_SUCCESS <- (%i) [%i %s] %s for REGISTER %s", 
				event->rid, event->status_code, event->reason_phrase, 
				event->remote_uri, event->req_uri);
			callmanager->handleRemoteEvent(0,NULL,EXOSIP_REGISTRATION_SUCCESS);
			break;

		case EXOSIP_REGISTRATION_FAILURE:
			qDebug("REGISTRATION_FAILURE <- (%i) [%i %s] %s for REGISTER %s", 
				event->rid, event->status_code, event->reason_phrase, 
				event->remote_uri, event->req_uri);
			if (countReg <= 3) { 
				setRegister();
				countReg++;
			}
			callmanager->handleRemoteEvent(0,NULL,EXOSIP_REGISTRATION_FAILURE);
			break;

		case EXOSIP_OPTIONS_NEW:
			qDebug("OPT_NEW <- (%i %i) OPTIONS from: %s", event->cid, 
				event->did, event->remote_uri);

			/* answer the OPTIONS method */
			/* 1: search for an existing call */
			int k;
			for (k = 0; k < NUMBER_OF_LINES; k++) {
				if (call[k] != NULL) {
					if (call[k]->cid == event->cid) {
						break;
					}
				}
			}
				// TODO: Que faire si rien trouve??
			eXosip_lock();
			if (call[k]->cid == event->cid) {
				/* already answered! */
			}
			else if (k == NUMBER_OF_LINES) {
				/* answer 200 ok */
				eXosip_answer_options (event->cid, event->did, 200);
			} else {
				/* answer 486 ok */
				eXosip_answer_options (event->cid, event->did, 486);
			}
			eXosip_unlock();
			break;

		case EXOSIP_OPTIONS_ANSWERED:
			qDebug("OPT_ANSWERED <- (%i %i) [%i %s] %s", event->cid, 
				event->did, event->status_code, event->reason_phrase, 
				event->remote_uri);
			break;

		case EXOSIP_OPTIONS_PROCEEDING:
			qDebug("OPT_PROCEEDING <- (%i %i) [%i %s] %s", event->cid, 
				event->did, event->status_code, event->reason_phrase, 
				event->remote_uri);
			break;

		case EXOSIP_OPTIONS_REDIRECTED:
			qDebug("OPT_REDIR <- (%i %i) [%i %s] %s", event->cid, 
				event->did, event->status_code, event->reason_phrase, 
				event->remote_uri);
			break;

		case EXOSIP_OPTIONS_REQUESTFAILURE:
			qDebug ("OPT_REQ_FAIL <- (%i %i) [%i %s] %s", event->cid, 
				event->did, event->status_code, event->reason_phrase, 
				event->remote_uri);
			break;

		case EXOSIP_OPTIONS_SERVERFAILURE:
			qDebug("OPT_SVR_FAIL <- (%i %i) [%i %s] %s", event->cid, 
				event->did, event->status_code, event->reason_phrase, 
				event->remote_uri);
			break;

		case EXOSIP_OPTIONS_GLOBALFAILURE:
			qDebug("OPT_GL_FAIL <- (%i %i) [%i %s] %s", event->cid, 
				event->did, event->status_code, event->reason_phrase, 
				event->remote_uri);
			break;

		case EXOSIP_SUBSCRIPTION_NOTIFY:
			qDebug("SUBS_NOTIFY <- (%i %i) NOTIFY from: %s", event->sid, 
				event->did, event->remote_uri);
			qDebug("<- (%i %i) online=%i [status: %i reason:%i]",
				event->sid, event->did, event->online_status, 
				event->ss_status, event->ss_reason);
			//subscription_notify(je);
			break;

		default:
			return -1;
			break;
	}
	eXosip_event_free(event);
 	
    return 0;
}
