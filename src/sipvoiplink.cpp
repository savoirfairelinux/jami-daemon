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
#include <sys/time.h>

#include <eXosip/eXosip.h>
#include <osip2/osip.h>
#include <osipparser2/osip_const.h>
#include <osipparser2/osip_headers.h>
#include <osipparser2/osip_body.h>

#include <cc++/thread.h>
#include <iostream>
#include <string>
#include <vector>

#include "audio/audiortp.h"
#include "call.h"
#include "eventthread.h"
#include "global.h"
#include "manager.h"
#include "sipcall.h"
#include "sipvoiplink.h"
#include "user_cfg.h"
#include "voIPLink.h"
 
using namespace ost;
using namespace std;

#define DEFAULT_SIP_PORT	5060
#define RANDOM_SIP_PORT		rand() % 64000 + 1024
#define	DEFAULT_LOCAL_PORT	10500
#define	RANDOM_LOCAL_PORT	((rand() % 27250) + 5250)*2


SipVoIPLink::SipVoIPLink (short id, Manager* manager) : VoIPLink (id, manager)
{
	setId(id);
	_localPort = 0;
	_manager = manager;
	_evThread = new EventThread (this);
	_sipcallVector = new SipCallVector();
	_audiortp = new AudioRtp(_manager);
}

SipVoIPLink::~SipVoIPLink (void)
{
	if (_evThread != NULL) {
		delete _evThread;
		_evThread = NULL;
	}
	delete _sipcallVector;
	delete _audiortp;
}

int
SipVoIPLink::init (void)
{
	string tmp;

	tmp = string(PROGNAME) + "/" + string(VERSION);

	// Set IP address
	if (getLocalIp() == -1)
		return -1;
	
	srand (time(NULL));
	if (eXosip_init (NULL, NULL, DEFAULT_SIP_PORT) != 0) {
		if (eXosip_init (NULL, NULL, RANDOM_SIP_PORT) != 0) {
			_debug("Cannot init eXosip\n");
			return -1;
		}
	}
	// If use STUN server, firewall address setup
	if (_manager->useStun()) {
		eXosip_set_user_agent(tmp.data());
		StunAddress4 stunSvrAddr;
		stunSvrAddr.addr = 0;
		
		// Stun server
		string svr = get_config_fields_str(SIGNALISATION, STUN_SERVER);
		
		// Convert char* to StunAddress4 structure
		bool ret = stunParseServerName ((char*)svr.data(), stunSvrAddr);
		if (!ret) {
			_debug("SIP: Stun server address not valid\n");
		}
		
		// Firewall address
		_debug("STUN server: %s\n", svr.data());
		_manager->getStunInfo(stunSvrAddr);
		eXosip_set_firewallip((_manager->getFirewallAddress()).data());
	} 
	
	eXosip_set_user_agent(tmp.data());
	_evThread->start();
	initRtpmapCodec();
	return 1;
}

bool
SipVoIPLink::isInRtpmap (int index, int payload, CodecDescriptorVector* cdv) {
    for (int i = 0; i < index; i++) {
        if (cdv->at(i)->getPayload() == payload) {
            return true;
        }
    }
    return false;
}

void
SipVoIPLink::initRtpmapCodec (void)
{
	int payload;
	unsigned int nb;
	char rtpmap[128];
	char tmp[64];

	bzero(rtpmap, 128);
	bzero(tmp, 64);
	
 	/* reset all payload to fit application capabilities */
  	eXosip_sdp_negotiation_remove_audio_payloads();
	
	// Set rtpmap according to the supported codec order
	nb = _manager->getNumberOfCodecs();
	for (unsigned int i = 0; i < nb; i++) {
		payload = _manager->getCodecDescVector()->at(i)->getPayload();

		// Add payload to rtpmap if it is not already added
		if (!isInRtpmap(i, payload, _manager->getCodecDescVector())) {
			snprintf(rtpmap, 127, "%d %s/%d", payload, 
				_manager->getCodecDescVector()->at(i)->rtpmapPayload(payload).data(), 
				SAMPLING_RATE);
			snprintf(tmp, 63, "%d", payload);
			
			eXosip_sdp_negotiation_add_codec( osip_strdup(tmp), NULL,
					   osip_strdup("RTP/AVP"), NULL, NULL, NULL, NULL,NULL,
					   osip_strdup(rtpmap));
		}
	}
}

void
SipVoIPLink::quit(void) 
{
	eXosip_quit();	
}

int
SipVoIPLink::setRegister (void) 
{
	int reg_id = -1;

	string proxy = "sip:" + get_config_fields_str(SIGNALISATION, PROXY);

	string hostname = "sip:" + get_config_fields_str(SIGNALISATION, HOST_PART);
	
	string from = fromHeader(get_config_fields_str(SIGNALISATION, USER_PART), 
							get_config_fields_str(SIGNALISATION, HOST_PART));

	if (get_config_fields_str(SIGNALISATION, HOST_PART).empty()) {
		_manager->error()->errorName(HOST_PART_FIELD_EMPTY, NULL);
		return -1;
	}
	if (get_config_fields_str(SIGNALISATION, USER_PART).empty()) {
		_manager->error()->errorName(USER_PART_FIELD_EMPTY, NULL);
		return -1;
	}

	eXosip_lock();
	if (setAuthentication() == -1) {
		eXosip_unlock();
		return -1;
	}
	
	_debug("register From: %s\n", from.data());
	if (!get_config_fields_str(SIGNALISATION, PROXY).empty()) {
		reg_id = eXosip_register_init((char*)from.data(), (char*)proxy.data(), 
				NULL);
	} else {
		reg_id = eXosip_register_init((char*)from.data(),(char*)hostname.data(),
			   	NULL);
	}
	if (reg_id < 0) {
		eXosip_unlock();
		return -1;
	}	
	
	// TODO: port SIP session timer dans config
	int i = eXosip_register(reg_id, EXPIRES_VALUE);
	if (i == -2) {
		_debug("cannot build registration, check the setup\n"); 
		eXosip_unlock();
		return -1;
	}
	if (i == -1) {
		_debug("Registration Failed\n");
		eXosip_unlock();
		return -1;
	}
	
	eXosip_unlock();
	return 0;
}
int
SipVoIPLink::outgoingInvite (const string& to_url) 
{
	string from;
	string to;

	// Form the From header field basis on configuration panel
	from = fromHeader(get_config_fields_str(SIGNALISATION, USER_PART),
							get_config_fields_str(SIGNALISATION, HOST_PART));
	
	to = toHeader(to_url);

	if (to.find("@") == string::npos and 
			get_config_fields_int(SIGNALISATION, AUTO_REGISTER) == YES) {
		to = to + "@" + get_config_fields_str(SIGNALISATION, HOST_PART);
	}
		
	_debug("From: %s\n", from.data());
	_debug("To: %s\n", to.data());

	if (get_config_fields_str(SIGNALISATION, PROXY).empty()) {
	// If no SIP proxy setting for direct call with only IP address
		if (startCall(from, to, "", "") <= 0) {
			_debug("Warning SipVoIPLink: call not started\n");
			return -1;
		}
		return 0;
	} else {
	// If SIP proxy setting
		string route = "<sip:" + 
			get_config_fields_str(SIGNALISATION, PROXY) + ";lr>";
		if (startCall(from, to, "", route) <= 0) {
			_debug("Warning SipVoIPLink: call not started\n");
			return -1;
		}
		return 0;
	}
}

int
SipVoIPLink::answer (short id) 
{
	int i;
	char tmpbuf[64];
	bzero (tmpbuf, 64);
    // Get local port   
    snprintf (tmpbuf, 63, "%d", getSipCall(id)->getLocalAudioPort());
	
	_debug("Answer call [id = %d, cid = %d, did = %d]\n", 
			id, getSipCall(id)->getCid(), getSipCall(id)->getDid());
	_debug("Local audio port: %d\n", getSipCall(id)->getLocalAudioPort());
	
	eXosip_lock();
    i = eXosip_answer_call(getSipCall(id)->getDid(), 200, tmpbuf);
	eXosip_unlock();

	// Incoming call is answered, start the sound channel.
	if (_audiortp->createNewSession (getSipCall(id)) < 0) {
		_debug("FATAL: Unable to start sound (%s:%d)\n", __FILE__, __LINE__);
		exit(1);
	}
	return i;
}

int
SipVoIPLink::hangup (short id) 
{
	int i = 1;
	if (!_manager->getbCongestion()) {
		_debug("Hang up call [id = %d, cid = %d, did = %d]\n", 
				id, getSipCall(id)->getCid(), getSipCall(id)->getDid());	
		// Release SIP stack.
		eXosip_lock();
		i = eXosip_terminate_call (getSipCall(id)->getCid(), 
									getSipCall(id)->getDid());
		eXosip_unlock();

		// Release RTP channels
		_audiortp->closeRtpSession(getSipCall(id));
	}
				
	deleteSipCall(id);
	return i;
}

int
SipVoIPLink::onhold (short id) 
{
	int i;
	
	eXosip_lock();
	i = eXosip_on_hold_call(getSipCall(id)->getDid());
	eXosip_unlock();

	// Disable audio
	_audiortp->closeRtpSession(getSipCall(id));
	return i;
}

int
SipVoIPLink::offhold (short id) 
{
	int i;
	
	eXosip_lock();
	i = eXosip_off_hold_call(getSipCall(id)->getDid(), NULL, 0);
	eXosip_unlock();

	// Enable audio
	if (_audiortp->createNewSession (getSipCall(id)) < 0) {
		_debug("FATAL: Unable to start sound (%s:%d)\n", __FILE__, __LINE__);
		exit(1);
	}
	return i;
}

int
SipVoIPLink::transfer (short id, const string& to)
{
	int i;
	string tmp_to;
	to = toHeader(to);
	if (to.find("@") == string::npos) {
		tmp_to = to + "@" + get_config_fields_str(SIGNALISATION, HOST_PART);
	}

	eXosip_lock();
	i = eXosip_transfer_call(getSipCall(id)->getDid(), (char*)tmp_to.data());
	eXosip_unlock();
	return i;
}

int
SipVoIPLink::refuse (short id)
{
	int i;
	char tmpbuf[64];
	bzero (tmpbuf, 64);
    // Get local port   
    snprintf (tmpbuf, 63, "%d", getSipCall(id)->getLocalAudioPort());
	
	eXosip_lock();
	i = eXosip_answer_call(getSipCall(id)->getDid(), BUSY_HERE, tmpbuf);
	eXosip_unlock();
	return i;
}

int
SipVoIPLink::getEvent (void)
{
	eXosip_event_t *event;
	short id;
	char *name;
	static int countReg = 0;

	event = eXosip_event_wait (0, 50);
	eXosip_automatic_refresh();
	if (event == NULL) {
		return -1;
	}	
	switch (event->type) {
		// IP-Phone user receives a new call
		case EXOSIP_CALL_NEW: //
			// Set local random port for incoming call
			setLocalPort(RANDOM_LOCAL_PORT);
			
			id = _manager->generateNewCallId();
			_manager->pushBackNewCall(id, Incoming);
			_debug("Incoming Call with identifiant %d [cid = %d, did = %d]\n",
				   id, event->cid, event->did);
			_debug("Local audio port: %d\n", _localPort);

			// Display the callerId-name
			osip_from_t *from;
  			osip_from_init(&from);
  			osip_from_parse(from, event->remote_uri);
			name = osip_from_get_displayname(from);
			_manager->displayTextMessage(id, name);
			_manager->getCall(id)->setCallerIdName(name);
			_debug("From: %s\n", name);
			osip_from_free(from);
			
			getSipCall(id)->newIncomingCall(event);
			_manager->incomingCall(id);

			// Associate an audio port with a call
			getSipCall(id)->setLocalAudioPort(_localPort);
			
			break;

		// The peer-user answers
		case EXOSIP_CALL_ANSWERED: 
			id = findCallId(event);
			if (id == 0) {
				id = findCallIdWhenRinging();
				getSipCall(id)->setLocalAudioPort(_localPort);
			}
			_debug("Call is answered [id = %d, cid = %d, did = %d]\n", 
					id, event->cid, event->did);
 
			// Answer
			if (id > 0 and !_manager->getCall(id)->isOnHold()
					   and !_manager->getCall(id)->isOffHold()) {
				getSipCall(id)->setStandBy(false);
				getSipCall(id)->answeredCall(event);
				_manager->peerAnsweredCall(id);

			
				// Outgoing call is answered, start the sound channel.
				if (_audiortp->createNewSession (getSipCall(id)) < 0) {
					_debug("FATAL: Unable to start sound (%s:%d)\n", 
							__FILE__, __LINE__);
					exit(1);
				}
			}
			break;

		case EXOSIP_CALL_RINGING: //peer call is ringing
			id = findCallIdWhenRinging();
			
			_debug("Call is ringing [id = %d, cid = %d, did = %d]\n", 
					id, event->cid, event->did);
			
			if (id > 0) {
				getSipCall(id)->setLocalAudioPort(_localPort);
				getSipCall(id)->ringingCall(event);
				_manager->peerRingingCall(id);
			} 
			break;

		case EXOSIP_CALL_REDIRECTED:
			break;

		// The peer-user closed the phone call(we received BYE).
		case EXOSIP_CALL_CLOSED:
			id = findCallId(event);
			_debug("Call is closed [id = %d, cid = %d, did = %d]\n", 
					id, event->cid, event->did);	
			
			if (id > 0) {	
				_manager->peerHungupCall(id);
				_audiortp->closeRtpSession(getSipCall(id));
				deleteSipCall(id);
			}	
			break;

		case EXOSIP_CALL_HOLD:
			id = findCallId(event);
			if (id > 0) {
				getSipCall(id)->onholdCall(event);
			}
			break;

		case EXOSIP_CALL_OFFHOLD:
			id = findCallId(event);
			if (id > 0) {
				getSipCall(id)->offholdCall(event);
			}
			break;
			
		case EXOSIP_CALL_REQUESTFAILURE:
			id = findCallId(event);

			// Handle 4XX errors
			switch (event->status_code) {
				case AUTH_REQUIRED:
					if (setAuthentication() == -1) {
						break;
					}
					eXosip_lock();
					eXosip_retry_call (event->cid);
					eXosip_unlock();
					break;
				case BAD_REQ:
				case UNAUTHORIZED:
				case FORBIDDEN:
				case NOT_FOUND:
				case NOT_ALLOWED:
				case NOT_ACCEPTABLE:
				case REQ_TIMEOUT:
				case TEMP_UNAVAILABLE:
				case ADDR_INCOMPLETE:
				case BUSY_HERE:
					// Display error on the screen phone
					_manager->displayError(event->reason_phrase);
					_manager->congestion(true);
					break;
				case REQ_TERMINATED:
					break;
				default:
					break;
			}
			break; 

		case EXOSIP_CALL_SERVERFAILURE:
			// Handle 5XX errors
			switch (event->status_code) {
				case SERVICE_UNAVAILABLE:
					_manager->ringback(false);
					_manager->congestion(true);					
					break;
				default:
					break;
			}
			break;

		case EXOSIP_CALL_GLOBALFAILURE:
			// Handle 6XX errors
			switch (event->status_code) {
				case BUSY_EVERYWHERE:
				case DECLINE:
					_manager->ringback(false);
					_manager->congestion(true);					
					break;
				default:
					break;
			}
			break;

		case EXOSIP_REGISTRATION_SUCCESS:
			_debug("-- Registration succeeded --\n");
			_manager->displayStatus(LOGGED_IN_STATUS);
			break;

		case EXOSIP_REGISTRATION_FAILURE:
			if (countReg <= 3) { 
				setRegister();
				countReg++;
			}
			break;

		case EXOSIP_OPTIONS_NEW:
			/* answer the OPTIONS method */
			/* 1: search for an existing call */
			unsigned int k;
				
			for (k = 0; k < _sipcallVector->size(); k++) {
				if (_sipcallVector->at(k)->getCid() == event->cid) { 
					break;
				}
			}
		
			// TODO: Que faire si rien trouve??
			eXosip_lock();
			if (_sipcallVector->at(k)->getCid() == event->cid) {
				/* already answered! */
			}
			else if (k == _sipcallVector->size()) {
				/* answer 200 ok */
				eXosip_answer_options (event->cid, event->did, 200);
			} else {
				/* answer 486 ok */
				eXosip_answer_options (event->cid, event->did, 486);
			}
			eXosip_unlock();
			break;

		case EXOSIP_OPTIONS_ANSWERED:
			break;

		case EXOSIP_OPTIONS_PROCEEDING:
			break;

		case EXOSIP_OPTIONS_REDIRECTED:
			break;

		case EXOSIP_OPTIONS_REQUESTFAILURE:
			break;

		case EXOSIP_OPTIONS_SERVERFAILURE:
			break;

		case EXOSIP_OPTIONS_GLOBALFAILURE:
			break;

		case EXOSIP_SUBSCRIPTION_NOTIFY:
			break;

		default:
			return -1;
			break;
	}
	eXosip_event_free(event);
 	
    return 0;
}

int
SipVoIPLink::getLocalPort (void) 
{
	return _localPort;
}

void
SipVoIPLink::setLocalPort (int port) 
{
	_localPort = port;
}

void
SipVoIPLink::carryingDTMFdigits (short id, char code) {
	  int duration = get_config_fields_int(SIGNALISATION, PULSE_LENGTH);
      
	  static const int body_len = 128;

      char *dtmf_body = new char[body_len];
	  snprintf(dtmf_body, body_len - 1,
			  "Signal=%c\r\nDuration=%d\r\n",
			  code, duration);

      eXosip_lock();
      eXosip_info_call(getSipCall(id)->getDid(), "application/dtmf-relay", 
			  dtmf_body);
      eXosip_unlock();
	
	  delete[] dtmf_body;
}
 
void
SipVoIPLink::newOutgoingCall (short callid)
{
	_sipcallVector->push_back(new SipCall(callid, 
										  _manager->getCodecDescVector()));
	if (getSipCall(callid) != NULL) {
		getSipCall(callid)->setStandBy(true);
	}
}

void
SipVoIPLink::newIncomingCall (short callid)
{
	SipCall* sipcall = new SipCall(callid, _manager->getCodecDescVector());
	_sipcallVector->push_back(sipcall);
}

void
SipVoIPLink::deleteSipCall (short callid)
{
	unsigned int i = 0;
	while (i < _sipcallVector->size()) {
		if (_sipcallVector->at(i)->getId() == callid) {
			_sipcallVector->erase(_sipcallVector->begin()+i);
			return;
		} else {
			i++;
		}
	}
}

SipCall*
SipVoIPLink::getSipCall (short callid)
{
	for (unsigned int i = 0; i < _sipcallVector->size(); i++) {
		if (_sipcallVector->at(i)->getId() == callid) {
			return _sipcallVector->at(i);
		} 
	}
	return NULL;
}

AudioCodec*
SipVoIPLink::getAudioCodec (short callid)
{
	return getSipCall(callid)->getAudioCodec();
}
///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

int 
SipVoIPLink::getLocalIp (void) 
{
	char* myIPAddress;
	if (getLocalIpAddress().empty()) {
		myIPAddress = new char[64];
	}
	int ret = eXosip_guess_localip (2, myIPAddress, 64);
	setLocalIpAddress(string(myIPAddress));

	return ret;
}

int
SipVoIPLink::checkUrl (const string& url)
{
  	int i;
	
  	osip_from_t *to;
  	i = osip_from_init(&to);
  	if (i != 0) {
		_debug("Warning: Cannot initialize\n");
		return -1;
	}
  	i = osip_from_parse(to, url.data());
  	if (i != 0) {
		_debug("Warning: Cannot parse url\n");
		return -1;
	}

	// Free memory
	osip_from_free (to);
  	return 0;
}

int
SipVoIPLink::setAuthentication (void) 
{
	string login, pass, realm;
	login = get_config_fields_str(SIGNALISATION, AUTH_USER_NAME);
	if (login.empty()) {
		login = get_config_fields_str(SIGNALISATION, USER_PART);
	}
	pass = get_config_fields_str(SIGNALISATION, PASSWORD);
	if (pass.empty()) {
		_manager->error()->errorName(PASSWD_FIELD_EMPTY, NULL);				
		return -1;
	}

	if (eXosip_add_authentication_info(login.data(), login.data(), 
		pass.data(), NULL, NULL) != 0) {
		_debug("No authentication\n");
		return -1;
	}
	return 0;
}

string
SipVoIPLink::fromHeader (const string& user, const string& host) 
{
	string displayname = get_config_fields_str(SIGNALISATION, FULL_NAME);
	return ("\"" + displayname + "\"" + " <sip:" + user + "@" + host + ">");
}


string
SipVoIPLink::toHeader(const string& to) 
{
	if (to.find("sip:") == string::npos) {
		return ("sip:" + to );
	} else {
		return to;
	}
}

int
SipVoIPLink::startCall (const string& from, const string& to, 
							const string& subject,  const string& route) 
{
  	osip_message_t *invite;
  	int i;

  	if (checkUrl(from) != 0) {
		_manager->error()->errorName(FROM_ERROR, NULL);
    	return -1;
  	}
  	if (checkUrl(to) != 0) {
		_manager->error()->errorName(TO_ERROR, NULL);
    	return -1;
  	}
  	
	i = eXosip_build_initial_invite(&invite,(char*)to.data(),(char*)from.data(),
		   						(char*)route.data(), (char*)subject.data());	
  	if (i != 0) {
		return -1;
    }
  	
	eXosip_lock();
	
	char port[64];
	if (!_manager->useStun()) {
		// Set random port for outgoing call
		setLocalPort(RANDOM_LOCAL_PORT);
		_debug("Local audio port: %d\n",_localPort);

		bzero (port, 64);
		snprintf (port, 63, "%d", getLocalPort());
		
  		i = eXosip_initiate_call (invite, NULL, NULL, port);
		
	} else {
		// If use Stun server
		bzero (port, 64);
		snprintf (port, 63, "%d", _manager->getFirewallPort());
		
		i = eXosip_initiate_call(invite, NULL, NULL, port);

		_debug("sip invite: firewall port = %s\n", port);
	}

	if (i <= 0) {
		eXosip_unlock();
		return -1;
	}

	eXosip_unlock();
  	return i;	
}

short
SipVoIPLink::findCallId (eXosip_event_t *e)
{
	unsigned int k;
	
	for (k = 0; k < _sipcallVector->size(); k++) {
		if (_sipcallVector->at(k)->getCid() == e->cid and
				_sipcallVector->at(k)->getDid()	== e->did) {
			return _sipcallVector->at(k)->getId();
		}
    }
	return 0;
}

short
SipVoIPLink::findCallIdWhenRinging (void)
{
	unsigned int k;
	int i = _manager->selectedCall();
	
	if (i != -1) {
		return i;
	} else {
		for (k = 0; k < _sipcallVector->size(); k++) {
			if (_sipcallVector->at(k)->getStandBy()) {
				return _sipcallVector->at(k)->getId();
			}
		}
	}
	return 0;
}
