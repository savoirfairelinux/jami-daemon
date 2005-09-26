/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *                                                                              
 *  Portions Copyright (C) 2002,2003   Aymeric Moizard <jack@atosc.org>
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
//#include <sys/time.h>

#include <eXosip2/eXosip.h>  
#include <osip2/osip.h>

#include <string>

#include "sipvoiplink.h"
#include "global.h"
#include "audio/codecDescriptor.h"
#include "error.h"
#include "manager.h"
#include "sipcall.h"
#include "user_cfg.h"
#include "eventthread.h"

 
using namespace ost;
using namespace std;

#define DEFAULT_SIP_PORT	5060
#define RANDOM_SIP_PORT		rand() % 64000 + 1024
#define	DEFAULT_LOCAL_PORT	10500
#define	RANDOM_LOCAL_PORT	((rand() % 27250) + 5250)*2

#define VOICE_MSG			"Voice-Message"
#define LENGTH_VOICE_MSG	15

SipVoIPLink::SipVoIPLink (short id) 
  : VoIPLink (id)
{
  setId(id);
  _localPort = 0;
  _reg_id = -1;
  _nMsgVoicemail = 0;
  _evThread = new EventThread(this);
}

SipVoIPLink::~SipVoIPLink(void) {
  delete _evThread;
}

bool 
SipVoIPLink::checkNetwork (void) 
{
  // Set IP address
  if (getLocalIp() == -1) {
    // If no network
    return false;
  } else {
    return true;
  }
}

int
SipVoIPLink::init (void)
{
  string tmp;
  int i;

  tmp = string(PROGNAME) + "/" + string(VERSION);
	
  i = eXosip_init ();
  if (i != 0) {
    _debug("Could not initialize eXosip\n");
    exit (0);
  }
	
  srand (time(NULL));
  i = eXosip_listen_addr(IPPROTO_UDP, NULL, DEFAULT_SIP_PORT, AF_INET, 0);
  if (i != 0) {
    i = eXosip_listen_addr(IPPROTO_UDP, NULL, RANDOM_SIP_PORT, AF_INET, 0);
    if (i != 0) {
      _debug("Could not initialize transport layer\n");
      return -1;
    }
  }

  // If use STUN server, firewall address setup
  if (Manager::instance().useStun()) {
    eXosip_set_user_agent(tmp.data());
    if (behindNat() != 1) {
      return 0;
    }

    eXosip_masquerade_contact((Manager::instance().getFirewallAddress()).data(),
			      Manager::instance().getFirewallPort());
		
  } 
	
  // Set user agent
  eXosip_set_user_agent(tmp.data());
	
  _evThread->start();
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
SipVoIPLink::terminate(void) 
{
  delete _evThread;
  eXosip_quit();	
}

int
SipVoIPLink::setRegister (void) 
{
  _debug("SipVoIPLink::setRegister()\n");
  int i;
  osip_message_t *reg = NULL;

  string proxy = "sip:" + get_config_fields_str(SIGNALISATION, PROXY);

  string hostname = "sip:" + get_config_fields_str(SIGNALISATION, HOST_PART);
	
  string from = fromHeader(get_config_fields_str(SIGNALISATION, USER_PART), 
			   get_config_fields_str(SIGNALISATION, HOST_PART));

  if (get_config_fields_str(SIGNALISATION, HOST_PART).empty()) {
    Manager::instance().error()->errorName(HOST_PART_FIELD_EMPTY);
    return -1;
  }
  if (get_config_fields_str(SIGNALISATION, USER_PART).empty()) {
    Manager::instance().error()->errorName(USER_PART_FIELD_EMPTY);
    return -1;
  }

  eXosip_lock();
  if (setAuthentication() == -1) {
    _debug("No authentication\n");
    eXosip_unlock();
    return -1;
  }
	
  _debug("REGISTER From: %s\n", from.data());
  if (!get_config_fields_str(SIGNALISATION, PROXY).empty()) {
    _reg_id = eXosip_register_build_initial_register ((char*)from.data(), 
						      (char*)proxy.data(), NULL, EXPIRES_VALUE, &reg);
  } else {
    _reg_id = eXosip_register_build_initial_register ((char*)from.data(), 
						      (char*)hostname.data(), NULL, EXPIRES_VALUE, &reg);
  }
  if (_reg_id < 0) {
    eXosip_unlock();
    return -1;
  }	

  i = eXosip_register_send_register (_reg_id, reg);
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

  Manager::instance().error()->setError(0);

  return i;
}

int 
SipVoIPLink::setUnregister (void)
{
  int i;
  //	int reg_id = -1;
  osip_message_t *reg = NULL;

  eXosip_lock();

  if (_reg_id > 0) {
    _debug("UNREGISTER\n");
    i = eXosip_register_build_register (_reg_id, 0, &reg);
  }
	
  if (_reg_id < 0) {
    eXosip_unlock();
    return -1;
  }	
	
  i = eXosip_register_send_register (_reg_id, reg);
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

  Manager::instance().error()->setError(0);
  return i;
}

int
SipVoIPLink::outgoingInvite (short id, const string& to_url) 
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
    if (checkNetwork()) {
      if (startCall(id, from, to, "", "") <= 0) {
	_debug("Warning SipVoIPLink: call not started\n");
	return -1;
      }
    } else {
      Manager::instance().displayErrorText(id, "No network found\n");
      return -1;
    }
    return 0;
  } else {
    // If SIP proxy setting
    string route = "<sip:" + 
      get_config_fields_str(SIGNALISATION, PROXY) + ";lr>";
    if (checkNetwork()) {
      if (startCall(id, from, to, "", route) <= 0) {
	_debug("Warning SipVoIPLink: call not started\n");
	return -1;
      }
    } else {
      Manager::instance().displayErrorText(id, "No network found\n");
      return -1;
    }
    return 0;
  }
}

int
SipVoIPLink::answer (short id) 
{
  int i;
  int port;
  char tmpbuf[64];
  bzero (tmpbuf, 64);
  // Get  port   
  snprintf (tmpbuf, 63, "%d", getSipCall(id)->getLocalAudioPort());
	
  _debug("Answer call [id = %d, cid = %d, did = %d]\n", 
	 id, getSipCall(id)->getCid(), getSipCall(id)->getDid());
  port = getSipCall(id)->getLocalAudioPort();
  _debug("Local audio port: %d\n", port);
	
  
  osip_message_t *answer = NULL;
  SipCall* ca = getSipCall(id);

  // Send 180 RINGING
  eXosip_lock ();
  eXosip_call_send_answer (ca->getTid(), RINGING, NULL);
  eXosip_unlock ();

  // Send 200 OK
  eXosip_lock();
  i = eXosip_call_build_answer (ca->getTid(), OK, &answer);
  if (i != 0) {
    // Send 400 BAD_REQUEST
    eXosip_call_send_answer (ca->getTid(), BAD_REQ, NULL);
  } else {
    i = sdp_complete_200ok (ca->getDid(), answer, port);
    if (i != 0) {
      osip_message_free (answer);
      // Send 415 UNSUPPORTED_MEDIA_TYPE
      eXosip_call_send_answer (ca->getTid(), UNSUP_MEDIA_TYPE, NULL);
    } else {
      eXosip_call_send_answer (ca->getTid(), OK, answer);
    }
  }
  eXosip_unlock();

  // Incoming call is answered, start the sound channel.
  if (_audiortp.createNewSession (getSipCall(id)) < 0) {
    _debug("FATAL: Unable to start sound (%s:%d)\n", __FILE__, __LINE__);
    exit(1);
  }
  return i;
}

int
SipVoIPLink::hangup (short id) 
{
  int i = 0;
  if (!Manager::instance().getbCongestion()) {
    _debug("Hang up call [id = %d, cid = %d, did = %d]\n", 
	   id, getSipCall(id)->getCid(), getSipCall(id)->getDid());	
    // Release SIP stack.
    eXosip_lock();
    i = eXosip_call_terminate (getSipCall(id)->getCid(), 
			       getSipCall(id)->getDid());
    eXosip_unlock();

    // Release RTP channels
    _audiortp.closeRtpSession(getSipCall(id));
  }
				
  deleteSipCall(id);
  return i;
}

int
SipVoIPLink::cancel (short id) 
{
  int i = 0;
  if (!Manager::instance().getbCongestion()) {
    SipCall *call = getSipCall(id);
    _debug("Cancel call [id = %d, cid = %d]\n", id, call->getCid());
    // Release SIP stack.
    eXosip_lock();
    i = eXosip_call_terminate (call->getCid(), -1);
    eXosip_unlock();
  }
  deleteSipCall(id);
  return i;
}

int
SipVoIPLink::onhold (short id) 
{
  osip_message_t *invite;
  int i;
  int did;

  sdp_message_t *local_sdp = NULL;

  did = getSipCall(id)->getDid();
	
  eXosip_lock ();
  local_sdp = eXosip_get_local_sdp (did);
  eXosip_unlock ();
	
  if (local_sdp == NULL) {
    return -1;
  }

  eXosip_lock ();
  // Build INVITE_METHOD for put call on-hold
  i = eXosip_call_build_request (did, INVITE_METHOD, &invite);
  eXosip_unlock ();

  if (i != 0) {
    sdp_message_free(local_sdp);
    return -1;
  }

  /* add sdp body */
  {
    char *tmp = NULL;
    
    i = sdp_hold_call (local_sdp);
    if (i != 0) {
      sdp_message_free (local_sdp);
      osip_message_free (invite);
      return -1;
    }
    
    i = sdp_message_to_str (local_sdp, &tmp);
    sdp_message_free (local_sdp);
    if (i != 0) {
      osip_message_free (invite);
      osip_free (tmp);
      return -1;
    }
    osip_message_set_body (invite, tmp, strlen (tmp));
    osip_free (tmp);
    osip_message_set_content_type (invite, "application/sdp");
  }
  
  eXosip_lock ();
  // Send request
  i = eXosip_call_send_request (did, invite);
  eXosip_unlock ();
  
  // Disable audio
  _audiortp.closeRtpSession(getSipCall(id));
  return i;
}

int
SipVoIPLink::offhold (short id) 
{
  osip_message_t *invite;
  int i;
  int did;

  sdp_message_t *local_sdp = NULL;

  did = getSipCall(id)->getDid();
  eXosip_lock ();
  local_sdp = eXosip_get_local_sdp (did);
  eXosip_unlock ();
  if (local_sdp == NULL) {
    return -1;
  }

  eXosip_lock ();
  // Build INVITE_METHOD for put call off-hold
  i = eXosip_call_build_request (did, INVITE_METHOD, &invite);
  eXosip_unlock ();

  if (i != 0) {
    sdp_message_free(local_sdp);
    return -1;
  }

  /* add sdp body */
  {
    char *tmp = NULL;
    
    i = sdp_off_hold_call (local_sdp);
    if (i != 0) {
      sdp_message_free (local_sdp);
      osip_message_free (invite);
      return -1;
    }
    
    i = sdp_message_to_str (local_sdp, &tmp);
    sdp_message_free (local_sdp);
    if (i != 0) {
      osip_message_free (invite);
      osip_free (tmp);
      return -1;
    }
    osip_message_set_body (invite, tmp, strlen (tmp));
    osip_free (tmp);
    osip_message_set_content_type (invite, "application/sdp");
  }

  eXosip_lock ();
  // Send request
  i = eXosip_call_send_request (did, invite);
  eXosip_unlock ();
  
  // Enable audio
  if (_audiortp.createNewSession (getSipCall(id)) < 0) {
    _debug("FATAL: Unable to start sound (%s:%d)\n", __FILE__, __LINE__);
    exit(1);
  }
  return i;
}

int
SipVoIPLink::transfer (short id, const string& to)
{
  osip_message_t *refer;
  int i;
  string tmp_to;
  tmp_to = toHeader(to);
  if (tmp_to.find("@") == string::npos) {
    tmp_to = tmp_to + "@" + get_config_fields_str(SIGNALISATION, HOST_PART);
  }

  eXosip_lock();
  // Build transfer request
  i = eXosip_call_build_refer (getSipCall(id)->getDid(), (char*)tmp_to.data(),
			       &refer);
  if (i == 0) {
    // Send transfer request
    i = eXosip_call_send_request (getSipCall(id)->getDid(), refer);
  }
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
	
  osip_message_t *answer = NULL;
  eXosip_lock();
  // not BUSY.. where decline the invitation!
  i = eXosip_call_build_answer (getSipCall(id)->getTid(), SIP_DECLINE, &answer);
  if (i == 0) {
    i = eXosip_call_send_answer (getSipCall(id)->getTid(), SIP_DECLINE, answer);
  }
  eXosip_unlock();
  return i;
}

int
SipVoIPLink::getEvent (void)
{
  eXosip_event_t *event;
  short id;
  char *name;

  event = eXosip_event_wait (0, 50);
  eXosip_lock();
  eXosip_automatic_action();
  eXosip_unlock();

  if (event == NULL) {
    return -1;
  }

  _debug("GetEvent : %d\n", event->type);

  switch (event->type) {
    // IP-Phone user receives a new call
  case EXOSIP_CALL_INVITE: //
    // Set local random port for incoming call
    if (!Manager::instance().useStun()) {
      setLocalPort(RANDOM_LOCAL_PORT);
    } else {
      // If there is a firewall
      if (behindNat() != 0) {
	setLocalPort(Manager::instance().getFirewallPort());
      } else {
	return -1;
      }	
    }
			
    // Generate id
    id = Manager::instance().generateNewCallId();
    Manager::instance().pushBackNewCall(id, Incoming);
    _debug("Incoming Call with id %d [cid = %d, did = %d]\n",
	   id, event->cid, event->did);
    _debug("Local audio port: %d\n", _localPort);

    // Display the callerId-name
    osip_from_t *from;
    osip_from_init(&from);

    if (event->request != NULL) {
      char *tmp = NULL;

      osip_from_to_str (event->request->from, &tmp);
      if (tmp != NULL) {
	snprintf (getSipCall(id)->getRemoteUri(), 256, "%s", tmp);
	osip_free (tmp);
      }
    }
    osip_from_parse(from, getSipCall(id)->getRemoteUri());
    name = osip_from_get_displayname(from);
    Manager::instance().displayTextMessage(id, name);
    if (Manager::instance().getCall(id) != NULL) {
      Manager::instance().getCall(id)->setCallerIdName(name);
    } else {
      return -1;
    }
    _debug("From: %s\n", name);
    osip_from_free(from);
			
    // Associate an audio port with a call
    getSipCall(id)->setLocalAudioPort(_localPort);

			
    getSipCall(id)->newIncomingCall(event);
    if (Manager::instance().incomingCall(id) < 0) {
      Manager::instance().displayErrorText(id, "Incoming call failed");
      return -1;
    }
	
    break;

  case EXOSIP_CALL_REINVITE:
    eXosip_call_send_answer(event->tid, 403, NULL);
    break;

    // The peer-user answers
  case EXOSIP_CALL_ANSWERED: 
    id = findCallId(event);
    if (id == 0) {
      id = findCallIdInitial(event);
    }
    SipCall *call = getSipCall(id);
    if ( call ) {
      _debug("Call is answered [id = %d, cid = %d, did = %d], localport=%d\n", 
	   id, event->cid, event->did,call->getLocalAudioPort());
    }
 
    // Answer
    if (id > 0 && !Manager::instance().getCall(id)->isOnHold()
               && !Manager::instance().getCall(id)->isOffHold()) {
      call->setStandBy(false);
      if (call->answeredCall(event) != -1) {
        call->answeredCall_without_hold(event);
        Manager::instance().peerAnsweredCall(id);

        // Outgoing call is answered, start the sound channel.
        if (_audiortp.createNewSession (call) < 0) {
          _debug("FATAL: Unable to start sound (%s:%d)\n", 
          __FILE__, __LINE__);
          exit(1);
        }
      }
    } else {
      // Answer to on/off hold to send ACK
      if (id > 0) {
        call->answeredCall(event);
        _debug("-----------------------\n");
      }
    }
    break;
			
  case EXOSIP_CALL_RINGING: //peer call is ringing
    id = findCallId(event);
    //id = findCallIdWhenRinging();
			
    _debug("Call is ringing [id = %d, cid = %d, did = %d]\n", 
	   id, event->cid, event->did);
			
    if (id > 0) {
      getSipCall(id)->ringingCall(event);
      Manager::instance().peerRingingCall(id);
    } else {
      return -1;
    }
    break;

  case EXOSIP_CALL_REDIRECTED:
    break;

  case EXOSIP_CALL_ACK:
    id = findCallId(event);
    _debug("ACK received [id = %d, cid = %d, did = %d]\n", 
	   id, event->cid, event->did);
    if (id > 0) {
      getSipCall(id)->receivedAck(event);
    } else {
      return -1;
    }
    break;
			
    // The peer-user closed the phone call(we received BYE).
  case EXOSIP_CALL_CLOSED:
    id = findCallId(event);
    _debug("Call is closed [id = %d, cid = %d, did = %d]\n", 
	   id, event->cid, event->did);	
			
    if (id > 0) {	
      if (!Manager::instance().getCall(id)->isProgressing()) {
	_audiortp.closeRtpSession(getSipCall(id));
      }
      Manager::instance().peerHungupCall(id);
      deleteSipCall(id);
    } else {
      return -1;
    }	
    break;
  case EXOSIP_CALL_RELEASED:
    id = findCallIdInitial(event);
    _debug("Id Released: %d\n", id);
    //TODO: find the id...
    //Manager::instance().displayErrorText(0, "getEvent:CallReleased");

    break;
  case EXOSIP_CALL_REQUESTFAILURE:
    id = findCallId(event);

    // Handle 4XX errors
    switch (event->response->status_code) {
    case AUTH_REQUIRED:
      _debug("EXOSIP_CALL_REQUESTFAILURE :: AUTH_REQUIRED\n");
      eXosip_lock();
      eXosip_automatic_action();
      eXosip_unlock();
      break;
    case UNAUTHORIZED:
      _debug("EXOSIP_CALL_REQUESTFAILURE :: UNAUTHORIZED\n");
      setAuthentication();
      break;

    case BAD_REQ:
    case FORBIDDEN:
    case NOT_FOUND:
    case NOT_ALLOWED:
    case NOT_ACCEPTABLE:
    case REQ_TIMEOUT:
    case TEMP_UNAVAILABLE:
    case ADDR_INCOMPLETE:
    case BUSY_HERE:
      // Display error on the screen phone
      //Manager::instance().displayError(event->response->reason_phrase);
      Manager::instance().displayErrorText(id, event->response->reason_phrase);
      Manager::instance().congestion(true);
      break;
    case REQ_TERMINATED:
      break;
    default:
      break;
    }

    break; 

  case EXOSIP_CALL_SERVERFAILURE:
    // Handle 5XX errors
    switch (event->response->status_code) {
    case SERVICE_UNAVAILABLE:
      Manager::instance().ringback(false);
      Manager::instance().congestion(true);					
      break;
    default:
      break;
    }
    break;

  case EXOSIP_CALL_GLOBALFAILURE:
    // Handle 6XX errors
    switch (event->response->status_code) {
    case BUSY_EVERYWHERE:
    case DECLINE:
      Manager::instance().ringback(false);
      Manager::instance().congestion(true);					
      break;
    default:
      break;
    }
    break;

  case EXOSIP_REGISTRATION_SUCCESS: //1
    Manager::instance().displayStatus(LOGGED_IN_STATUS);
    break;

  case EXOSIP_REGISTRATION_FAILURE: // 2
    Manager::instance().displayError("getEvent : Registration Failure\n");
    break;

  case EXOSIP_MESSAGE_NEW:
    unsigned int k;
				
    if (event->request != NULL && MSG_IS_OPTIONS(event->request)) {
      for (k = 0; k < _sipcallVector.size(); k++) {
	if (_sipcallVector.at(k)->getCid() == event->cid) { 
	  break;
	}
      }
			
      // TODO: Que faire si rien trouve??
      eXosip_lock();
      if (k == _sipcallVector.size()) {
	/* answer 200 ok */
	eXosip_options_send_answer (event->tid, OK, NULL);
      } else if (_sipcallVector.at(k)->getCid() == event->cid) {
	/* already answered! */
      } else {
	/* answer 486 ok */
	eXosip_options_send_answer (event->tid, BUSY_HERE, NULL);
      }
      eXosip_unlock();
    } 
			
    // Voice message 
    else if (event->request != NULL && MSG_IS_NOTIFY(event->request)){
      int ii;
      unsigned int pos;
      unsigned int pos_slash;
      string *str;
      string nb_msg;
      osip_body_t *body;

      // Get the message body
      ii = osip_message_get_body(event->request, 0, &body);
      if (ii != 0) {
        _debug("Cannot get body\n");
        return -1;
      }
				
      // Analyse message body
      str = new string(body->body);
      pos = str->find (VOICE_MSG);
				
      if (pos == string::npos) {
	     // If the string is not found
       delete str;
	     return -1;
      } 
				
      pos_slash = str->find ("/");
      nb_msg = str->substr(pos + LENGTH_VOICE_MSG, 
			   pos_slash - (pos + LENGTH_VOICE_MSG));

      // Set the number of voice-message
      setMsgVoicemail(atoi(nb_msg.data()));

      if (getMsgVoicemail() != 0) {
	// If there is at least one voice-message, start notification
        Manager::instance().startVoiceMessageNotification();
      } else {
	// Stop notification when there is 0 voice message
        Manager::instance().stopVoiceMessageNotification();
      }
      delete str;
    }
    break;

  default:
    //Manager::instance().displayErrorText(event->type, "getEvent:default");
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
  osip_message_t *info;
  const int body_len = 1000;
  int i;

  char *dtmf_body = new char[body_len];

  eXosip_lock();
  // Build info request
  i = eXosip_call_build_info (getSipCall(id)->getDid(), &info);
  if (i == 0) {
    snprintf(dtmf_body, body_len - 1, "Signal=%c\r\nDuration=%d\r\n",
	     code, duration);
    osip_message_set_content_type (info, "application/dtmf-relay");
    osip_message_set_body (info, dtmf_body, strlen (dtmf_body));
    // Send info request
    i = eXosip_call_send_request (getSipCall(id)->getDid(), info);
  }
  eXosip_unlock();
	
  delete[] dtmf_body;
}
 
void
SipVoIPLink::newOutgoingCall (short callid)
{
  _sipcallVector.push_back(new SipCall(callid, 
					Manager::instance().getCodecDescVector()));
  SipCall *call = getSipCall(callid);
  if ( call != NULL) {
    call->setStandBy(true);
  }
}

void
SipVoIPLink::newIncomingCall (short callid)
{
  SipCall* sipcall = new SipCall(callid, Manager::instance().getCodecDescVector());
  _sipcallVector.push_back(sipcall);
}

void
SipVoIPLink::deleteSipCall (short callid)
{
  unsigned int i = 0;
  while (i < _sipcallVector.size()) {
    if (_sipcallVector.at(i)->getId() == callid) {
      _sipcallVector.erase(_sipcallVector.begin()+i);
      return;
    } else {
      i++;
    }
  }
}

SipCall*
SipVoIPLink::getSipCall (short callid)
{
  for (unsigned int i = 0; i < _sipcallVector.size(); i++) {
    if (_sipcallVector.at(i)->getId() == callid) {
      return _sipcallVector.at(i);
    } 
  }
  return NULL;
}

AudioCodec*
SipVoIPLink::getAudioCodec (short callid)
{
  if (getSipCall(callid)) {
    return getSipCall(callid)->getAudioCodec();
  } else {
    return NULL;
  }
}
///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////
int
SipVoIPLink::sdp_hold_call (sdp_message_t * sdp)
{
  int pos;
  int pos_media = -1;
  char *rcvsnd;
  int recv_send = -1;

  pos = 0;
  rcvsnd = sdp_message_a_att_field_get (sdp, pos_media, pos);
  while (rcvsnd != NULL) {
    if (rcvsnd != NULL && 0 == strcmp (rcvsnd, "sendonly")) {
      recv_send = 0;
    } else if (rcvsnd != NULL && (0 == strcmp (rcvsnd, "recvonly")
				  || 0 == strcmp (rcvsnd, "sendrecv"))) {
      recv_send = 0;
      sprintf (rcvsnd, "sendonly");
    }
    pos++;
    rcvsnd = sdp_message_a_att_field_get (sdp, pos_media, pos);
  }

  pos_media = 0;
  while (!sdp_message_endof_media (sdp, pos_media)) {
    pos = 0;
    rcvsnd = sdp_message_a_att_field_get (sdp, pos_media, pos);
    while (rcvsnd != NULL) {
      if (rcvsnd != NULL && 0 == strcmp (rcvsnd, "sendonly")) {
	recv_send = 0;
      } else if (rcvsnd != NULL && (0 == strcmp (rcvsnd, "recvonly")
				    || 0 == strcmp (rcvsnd, "sendrecv"))) {
	recv_send = 0;
	sprintf (rcvsnd, "sendonly");
      }
      pos++;
      rcvsnd = sdp_message_a_att_field_get (sdp, pos_media, pos);
    }
    pos_media++;
  }

  if (recv_send == -1) {
    /* we need to add a global attribute with a field set to "sendonly" */
    sdp_message_a_attribute_add (sdp, -1, osip_strdup ("sendonly"), NULL);
  }

  return 0;
}

int
SipVoIPLink::sdp_off_hold_call (sdp_message_t * sdp)
{
  int pos;
  int pos_media = -1;
  char *rcvsnd;

  pos = 0;
  rcvsnd = sdp_message_a_att_field_get (sdp, pos_media, pos);
  while (rcvsnd != NULL) {
    if (rcvsnd != NULL && (0 == strcmp (rcvsnd, "sendonly")
			   || 0 == strcmp (rcvsnd, "recvonly"))) {
      sprintf (rcvsnd, "sendrecv");
    }
    pos++;
    rcvsnd = sdp_message_a_att_field_get (sdp, pos_media, pos);
  }

  pos_media = 0;
  while (!sdp_message_endof_media (sdp, pos_media)) {
    pos = 0;
    rcvsnd = sdp_message_a_att_field_get (sdp, pos_media, pos);
    while (rcvsnd != NULL) {
      if (rcvsnd != NULL && (0 == strcmp (rcvsnd, "sendonly")
			     || 0 == strcmp (rcvsnd, "recvonly"))) {
	sprintf (rcvsnd, "sendrecv");
      }
      pos++;
      rcvsnd = sdp_message_a_att_field_get (sdp, pos_media, pos);
    }
    pos_media++;
  }

  return 0;
}

int
SipVoIPLink::sdp_complete_200ok (int did, osip_message_t * answer, int port)
{
  sdp_message_t *remote_sdp;
  sdp_media_t *remote_med;
  char *tmp = NULL;
  char buf[4096];
  char port_tmp[64];
  int pos;

  char localip[128];

  // Format port to a char*
  bzero(port_tmp, 64);
  snprintf(port_tmp, 63, "%d", port);

  remote_sdp = eXosip_get_remote_sdp (did);
  if (remote_sdp == NULL) {
    return -1;                /* no existing body? */
  }

  eXosip_guess_localip (AF_INET, localip, 128);
  snprintf (buf, 4096,
	    "v=0\r\n"
	    "o=user 0 0 IN IP4 %s\r\n"
	    "s=session\r\n" "c=IN IP4 %s\r\n" "t=0 0\r\n", localip,localip);

  pos = 0;
  while (!osip_list_eol (remote_sdp->m_medias, pos)) {
    char payloads[128];
    int pos2;

    memset (payloads, '\0', sizeof (payloads));
    remote_med = (sdp_media_t *) osip_list_get (remote_sdp->m_medias, pos);

    if (0 == osip_strcasecmp (remote_med->m_media, "audio")) {
      pos2 = 0;
      while (!osip_list_eol (remote_med->m_payloads, pos2)) {
	tmp = (char *) osip_list_get (remote_med->m_payloads, pos2);
	if (tmp != NULL && (0 == osip_strcasecmp (tmp, "0")
			    || 0 == osip_strcasecmp (tmp, "8")
			    || 0 == osip_strcasecmp (tmp, "3"))) {
	  strcat (payloads, tmp);
	  strcat (payloads, " ");
	}
	pos2++;
      }
      strcat (buf, "m=");
      strcat (buf, remote_med->m_media);
      if (pos2 == 0 || payloads[0] == '\0') {
	strcat (buf, " 0 RTP/AVP \r\n");
	sdp_message_free (remote_sdp);
	return -1;        /* refuse anyway */
      } else {
	strcat (buf, " ");
	strcat (buf, port_tmp);
	strcat (buf, " RTP/AVP ");
	strcat (buf, payloads);
	strcat (buf, "\r\n");

	if (NULL != strstr (payloads, " 0 ")
	    || (payloads[0] == '0' && payloads[1] == ' ')) {
	  strcat (buf, "a=rtpmap:0 PCMU/8000\r\n");
	}
	if (NULL != strstr (payloads, " 8 ")
	    || (payloads[0] == '8' && payloads[1] == ' ')) {
	  strcat (buf, "a=rtpmap:8 PCMA/8000\r\n");
	}
	if (NULL != strstr (payloads, " 3")
	    || (payloads[0] == '3' && payloads[1] == ' ')) {
	  strcat (buf, "a=rtpmap:3 GSM/8000\r\n");
	}
      }
    } else {
      strcat (buf, "m=");
      strcat (buf, remote_med->m_media);
      strcat (buf, " 0 ");
      strcat (buf, remote_med->m_proto);
      strcat (buf, " \r\n");
    }
    pos++;
  }

  osip_message_set_body (answer, buf, strlen (buf));
  osip_message_set_content_type (answer, "application/sdp");
  sdp_message_free (remote_sdp);
  return 0;
}


int
SipVoIPLink::behindNat (void)
{
  StunAddress4 stunSvrAddr;
  stunSvrAddr.addr = 0;
	
  // Stun server
  string svr = get_config_fields_str(SIGNALISATION, STUN_SERVER);
	
  // Convert char* to StunAddress4 structure
  bool ret = stunParseServerName ((char*)svr.data(), stunSvrAddr);
  if (!ret) {
    _debug("SIP: Stun server address not valid\n");
    return 0;
  }
	
  // Firewall address
  _debug("STUN server: %s\n", svr.data());
  Manager::instance().getStunInfo(stunSvrAddr);

  return 1;
}

int 
SipVoIPLink::getLocalIp (void) 
{
  int ret = 0;
  char* myIPAddress = new char[65];
  ret = eXosip_guess_localip (2, myIPAddress, 64);
  setLocalIpAddress(std::string(myIPAddress));
  delete [] myIPAddress;
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
    Manager::instance().error()->errorName(PASSWD_FIELD_EMPTY);				
    return -1;
  }

  if (eXosip_add_authentication_info(login.data(), login.data(), 
				     pass.data(), NULL, NULL) != 0) {
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
SipVoIPLink::startCall (short id, const string& from, const string& to, 
			const string& subject,  const string& route) 
{
  SipCall *call = getSipCall(id);
  if ( call == NULL) {
    return -1; // error, we can't find the sipcall
  }
  osip_message_t *invite;
  int i;

  if (checkUrl(from) != 0) {
    Manager::instance().error()->errorName(FROM_ERROR);
    return -1;
  }
  if (checkUrl(to) != 0) {
    Manager::instance().error()->errorName(TO_ERROR);
    return -1;
  }
	
  char port[64];
  if (!Manager::instance().useStun()) {
    // Set random port for outgoing call if no firewall
    setLocalPort(RANDOM_LOCAL_PORT);
    _debug("Local audio port: %d\n",_localPort);
  } else {
    // If use Stun server
    if (behindNat() != 0) {
      _debug("sip invite: firewall port = %d\n",Manager::instance().getFirewallPort());	
      setLocalPort(Manager::instance().getFirewallPort());
    } else {
      return -1;
    }
  }
	
  // Set local audio port for sipcall(id)
  call->setLocalAudioPort(_localPort);
	
  bzero (port, 64);
  snprintf (port, 63, "%d", getLocalPort());
  	
  i = eXosip_call_build_initial_invite (&invite, (char*)to.data(),
                                        (char*)from.data(),
                                        (char*)route.data(),
                                        (char*)subject.data());
  if (i != 0) {
    return -1; // error when building the invite
  }

  int payload;
  unsigned int nb;
  char rtpmap[128];
  char rtpmap_attr[2048];
  char media[64];
  char media_audio[64];

  bzero(rtpmap, 128);
  bzero(rtpmap_attr, 2048);
  bzero(media, 64);
  bzero(media_audio, 64);
	
  // Set rtpmap according to the supported codec order
  nb = Manager::instance().getNumberOfCodecs();
  for (unsigned int i = 0; i < nb; i++) {
    payload = Manager::instance().getCodecDescVector()->at(i)->getPayload();

    // Add payload to rtpmap if it is not already added
    if (!isInRtpmap(i, payload, Manager::instance().getCodecDescVector())) {
      snprintf(media, 63, "%d ", payload);
      strcat (media_audio, media);
			
      snprintf(rtpmap, 127, "a=rtpmap: %d %s/%d\r\n", payload, 
	       Manager::instance().getCodecDescVector()->at(i)->rtpmapPayload(payload).data(), SAMPLING_RATE);
      strcat(rtpmap_attr, rtpmap);
    }
  }

  // http://www.antisip.com/documentation/eXosip2/group__howto1__initialize.html
  // tell sip if we support SIP extension like 100rel
  // osip_message_set_supported (invite, "100rel");

  /* add sdp body */
  {
    char tmp[4096];
    char localip[128];

    eXosip_guess_localip (AF_INET, localip, 128);
    snprintf (tmp, 4096,
              "v=0\r\n"
              "o=SFLphone 0 0 IN IP4 %s\r\n"
              "s=call\r\n"
              "c=IN IP4 %s\r\n"
              "t=0 0\r\n"
              "m=audio %s RTP/AVP %s\r\n"
	            "%s",
              localip, localip, port, media_audio, rtpmap_attr);
    // media_audio should be one, two or three numbers?
	  _debug("%s %d", tmp, strlen(tmp));
    osip_message_set_body (invite, tmp, strlen (tmp));
    osip_message_set_content_type (invite, "application/sdp");
  }
  
  eXosip_lock();
	
  // this is the cid (call id from exosip)
  int cid = eXosip_call_send_initial_invite (invite);

  // Keep the cid in case of cancelling
  call->setCid(cid);

  if (cid <= 0) {
    eXosip_unlock();
    return -1;
  } else {
    eXosip_call_set_reference (cid, NULL);
  }

  eXosip_unlock();

  return cid; // this is the Cid
}

short
SipVoIPLink::findCallId (eXosip_event_t *e)
{
  for (unsigned int k = 0; k < _sipcallVector.size(); k++) {
    SipCall *call = _sipcallVector.at(k);
    if (call->getCid() == e->cid &&
        call->getDid() == e->did) {
      return call->getId();
    }
  }
  return 0;
}

/**
 * This function is used when findCallId failed (return 0)
 * ie: the dialog id change
 *     can be use when anwsering a new call or 
 *         when cancelling a call
 */
short
SipVoIPLink::findCallIdInitial (eXosip_event_t *e)
{
  for (unsigned int k = 0; k < _sipcallVector.size(); k++) {
    SipCall *call = _sipcallVector.at(k);
    // the dialog id is not set when you do a new call
    // so you can't check it when you want to retreive it
    // for the first call anwser
    if (call->getCid() == e->cid) {
      return call->getId();
    }
  }
  return 0;
}
/**
 * YM: (2005-09-21) This function is really really bad..
 * Should be removed !
 * Don't ask the GUI which call it use...
 */
/*
short
SipVoIPLink::findCallIdWhenRinging (void)
{
  unsigned int k;
  int i = Manager::instance().selectedCall();
	
  if (i != -1) {
    return i;
  } else {
    for (k = 0; k < _sipcallVector.size(); k++) {
      if (_sipcallVector.at(k)->getStandBy()) {
	return _sipcallVector.at(k)->getId();
      }
    }
  }
  return 0;
}
*/
