/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#include <eXosip2/eXosip.h>
#include <osip2/osip.h>

#include "sipvoiplink.h"
#include "global.h"
#include "audio/codecDescriptor.h"
#include "manager.h"
#include "sipcall.h"
#include "user_cfg.h"
#include "eventthread.h"

#define DEFAULT_SIP_PORT	5060
#define RANDOM_SIP_PORT		rand() % 64000 + 1024
#define	DEFAULT_LOCAL_PORT	10500
#define	RANDOM_LOCAL_PORT	((rand() % 27250) + 5250)*2

#define VOICE_MSG			"Voice-Message"
#define LENGTH_VOICE_MSG	15

SipVoIPLink::SipVoIPLink() : VoIPLink()
{
  // default _audioRTP object initialization
  _evThread = new EventThread(this);
  _localPort = 0;
  _nMsgVoicemail = 0;
  _reg_id = -1;
  // defautlt _sipcallVector object initialization

  _registrationSend = false;
  _started = false;
}

SipVoIPLink::~SipVoIPLink(void) {
  endSipCalls();
  delete _evThread; _evThread = NULL;
  if (_started) {
    eXosip_quit();
  }
}

// for voIPLink interface
void
SipVoIPLink::terminate(void) 
{
}

bool
SipVoIPLink::checkNetwork (void)
{
  // Set IP address
  return getSipLocalIp();
}

bool
SipVoIPLink::init(void)
{
  if (0 != eXosip_init()) {
    _debug("Could not initialize eXosip\n");
    return false;
  }
  _started = true;

  srand (time(NULL));
  // second parameter, NULL is "::" for ipv6 and "0.0.0.0" for ipv4, we can put INADDR_ANY
  int i;
  i = eXosip_listen_addr(IPPROTO_UDP, INADDR_ANY, DEFAULT_SIP_PORT, AF_INET, 0);
  if (i != 0) {
    i = eXosip_listen_addr(IPPROTO_UDP, INADDR_ANY, RANDOM_SIP_PORT, AF_INET, 0);
    if (i != 0) {
      _debug("Could not initialize transport layer\n");
      return false;
    } else {
      _debug("VoIP Link listen on random port %d\n", RANDOM_SIP_PORT);
    }
  } else {
   _debug("VoIP Link listen on port %d\n", DEFAULT_SIP_PORT);
  }
  // Set user agent
  std::string tmp = std::string(PROGNAME_GLOBAL) + "/" + std::string(SFLPHONED_VERSION);
  eXosip_set_user_agent(tmp.data());

  // If use STUN server, firewall address setup
  if (Manager::instance().useStun()) {
    if (behindNat() != 1) {
      return false;
    }
    // This method is used to replace contact address with the public address of your NAT
    eXosip_masquerade_contact((Manager::instance().getFirewallAddress()).data(), Manager::instance().getFirewallPort());
  }

  if ( !checkNetwork() ) {
    return false;
  }
  _debug("SIP VoIP Link: listen to SIP Events\n");
  _evThread->start();
  return true;
}

/**
 * Subscibe to message-summary notify
 * It allows eXosip to not send ' 481 Subcription Does Not Exist ' response
 */
void
SipVoIPLink::subscribeMessageSummary()
{
  osip_message_t *subscribe;
  const char *route= NULL;

  // from/to
  ManagerImpl& manager = Manager::instance();
  std::string from = fromHeader(manager.getConfigString(SIGNALISATION, USER_PART), manager.getConfigString(SIGNALISATION, HOST_PART));

  // to
  std::string to;
  to = manager.getConfigString(SIGNALISATION, PROXY);
  if (!to.empty()) {
    to = toHeader(manager.getConfigString(SIGNALISATION, USER_PART)) + "@" + to;
  } else {
    to = from;
  }
  

  // like in http://www.faqs.org/rfcs/rfc3842.html
  const char *event="message-summary";
  int expires = 86400;

  // return 0 if no error
  // the first from is the to... but we send the same
  eXosip_lock();
  int error = eXosip_subscribe_build_initial_request(&subscribe, to.c_str(), from.c_str(), route, event, expires);
  eXosip_unlock();

  if (error == 0) {
    // Accept: application/simple-message-summary
    osip_message_set_header (subscribe, "Accept", "application/simple-message-summary");

    _debug("Sending Message-summary subscription");
    // return 0 if ok
    eXosip_lock();
    error = eXosip_subscribe_send_initial_request (subscribe);
    eXosip_unlock();
    _debug(" and return %d\n", error);
  }
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

int
SipVoIPLink::setRegister (void) 
{
  ManagerImpl& manager = Manager::instance();

  if (_reg_id != -1) {
    manager.displayError("Registration already sent. Try to unregister");
    return -1;
  }

  // all this will be inside the profil associate with the voip link
  std::string proxy = "sip:" + manager.getConfigString(SIGNALISATION, PROXY);
  std::string hostname = "sip:" + manager.getConfigString(SIGNALISATION, HOST_PART);
  std::string from = fromHeader(manager.getConfigString(SIGNALISATION, USER_PART), manager.getConfigString(SIGNALISATION, HOST_PART));

  if (manager.getConfigString(SIGNALISATION, HOST_PART).empty()) {
    manager.displayConfigError("Fill host part field");
    return -1;
  }
  if (manager.getConfigString(SIGNALISATION, USER_PART).empty()) {
    manager.displayConfigError("Fill user part field");
    return -1;
  }

  _debug("REGISTER From: %s\n", from.data());
  osip_message_t *reg = NULL;
  eXosip_lock();
  if (!manager.getConfigString(SIGNALISATION, PROXY).empty()) {
    _reg_id = eXosip_register_build_initial_register ((char*)from.data(), 
						      (char*)proxy.data(), NULL, EXPIRES_VALUE, &reg);
  } else {
    _reg_id = eXosip_register_build_initial_register ((char*)from.data(), 
						      (char*)hostname.data(), NULL, EXPIRES_VALUE, &reg);
  }
  eXosip_unlock();
  if (_reg_id < 0) {
    return -1;
  }

  if (setAuthentication() == -1) {
    _debug("No authentication\n");
    return -1;
  }

  osip_message_set_header (reg, "Event", "Registration");
  osip_message_set_header (reg, "Allow-Events", "presence");

  eXosip_lock();
  int i = eXosip_register_send_register (_reg_id, reg);
  if (i == -2) {
    _debug("Cannot build registration, check the setup\n"); 
    eXosip_unlock();
    return -1;
  }
  if (i == -1) {
    _debug("Registration sending failed\n");
    eXosip_unlock();
    return -1;
  }
  eXosip_unlock();

  // subscribe to message one time?
  // subscribeMessageSummary();
  _registrationSend = true;
  return i;
}

/**
 * setUnregister 
 * unregister if we already send the first registration
 * @return -1 if there is an error
 */
int 
SipVoIPLink::setUnregister (void)
{
  if ( _registrationSend ) {
    int i = 0;
    osip_message_t *reg = NULL;

    eXosip_lock();

    if (_reg_id > 0) {
      i = eXosip_register_build_register (_reg_id, 0, &reg);
    }
    eXosip_unlock();
    if (i < 0) {
      return -1;
    }

    eXosip_lock();
    _debug("< Sending REGISTER (expire=0)\n");
    i = eXosip_register_send_register (_reg_id, reg);
    if (i == -2) {
      _debug("  Cannot build registration (unregister), check the setup\n"); 
      eXosip_unlock();
      return -1;
    }
    if (i == -1) {
      _debug("  Registration (unregister) Failed\n");
    }
    eXosip_unlock();
    _reg_id = -1;
    return i;
  } else {
    // no registration send before
    return -1;
  }
}

int
SipVoIPLink::outgoingInvite (CALLID id, const std::string& to_url) 
{
  bool has_ip = checkNetwork();
  std::string from;
  std::string to;

  // TODO: should be inside account settings
  ManagerImpl& manager = Manager::instance();
  // Form the From header field basis on configuration panel
  std::string host = manager.getConfigString(SIGNALISATION, HOST_PART);
  std::string hostFrom = host;
  if ( hostFrom.empty() ) {
    hostFrom = getLocalIpAddress();
  }
  from = fromHeader(manager.getConfigString(SIGNALISATION, USER_PART), hostFrom);
	
  to = toHeader(to_url);

  if (to.find("@") == std::string::npos and 
      manager.getConfigInt(SIGNALISATION, AUTO_REGISTER)) {
    if(!host.empty()) {
      to = to + "@" + manager.getConfigString(SIGNALISATION, HOST_PART);
    }
  }
		
  _debug("            From: %s\n", from.data());
  _debug("            To: %s\n", to.data());

  if (manager.getConfigString(SIGNALISATION, PROXY).empty()) {
    // If no SIP proxy setting for direct call with only IP address
    if (has_ip) {
      if (startCall(id, from, to, "", "") <= 0) {
    	 _debug("Warning SipVoIPLink: call not started\n");
	     return -1;
      }
    } else {
      manager.displayErrorText(id, "No network found\n");
      return -1;
    }
    return 0;
  } else {
    // If SIP proxy setting
    std::string route = "<sip:" + 
      manager.getConfigString(SIGNALISATION, PROXY) + ";lr>";
    if (has_ip) {
      if (startCall(id, from, to, "", route) <= 0) {
	     _debug("Warning SipVoIPLink: call not started\n");
	     return -1;
      }
    } else {
      manager.displayErrorText(id, "No network found\n");
      return -1;
    }
    return 0;
  }
}

/**
 * @return 0 is good, -1 is bad
 */
int
SipVoIPLink::answer (CALLID id) 
{
  int i;
  int port;
  char tmpbuf[64];
  bzero (tmpbuf, 64);
  // Get  port   
  snprintf (tmpbuf, 63, "%d", getSipCall(id)->getLocalAudioPort());

  _debug("%10d: Answer call [cid = %d, did = %d]\n", id, getSipCall(id)->getCid(), getSipCall(id)->getDid());
  port = getSipCall(id)->getLocalAudioPort();
  _debug("            Local audio port: %d\n", port);

  osip_message_t *answerMessage = NULL;
  SipCall* ca = getSipCall(id);

  // Send 180 RINGING
  _debug("< Send 180 Ringing\n");
  eXosip_lock ();
  eXosip_call_send_answer (ca->getTid(), RINGING, NULL);
  eXosip_unlock ();

  // Send 200 OK
  eXosip_lock();
  i = eXosip_call_build_answer (ca->getTid(), OK, &answerMessage);
  if (i != 0) {
   _debug("< Send 400 Bad Request\n");
    eXosip_call_send_answer (ca->getTid(), BAD_REQ, NULL);
  } else {
    // use exosip, bug locked
    i = sdp_complete_200ok (ca->getDid(), answerMessage, port);
    if (i != 0) {
      osip_message_free (answerMessage);
      _debug("< Send 415 Unsupported Media Type\n");
      eXosip_call_send_answer (ca->getTid(), UNSUP_MEDIA_TYPE, NULL);
    } else {
      _debug("< Send 200 OK\n");
      eXosip_call_send_answer (ca->getTid(), OK, answerMessage);
    }
  }
  eXosip_unlock();

  // Incoming call is answered, start the sound channel.
  _debug("          Starting AudioRTP\n");
  if (_audiortp.createNewSession (getSipCall(id)) < 0) {
    _debug("FATAL: Unable to start sound (%s:%d)\n", __FILE__, __LINE__);
    i = -1;
  }
  return i;
}


/**
 * @return > 0 is good, -1 is bad
 */
int
SipVoIPLink::hangup (CALLID id) 
{
  int i = 0;
  SipCall* sipcall = getSipCall(id);
  if (sipcall == NULL) { return -1; }
  _debug("%10d: Hang up call [cid = %d, did = %d]\n", 
    id, sipcall->getCid(), sipcall->getDid());	
  // Release SIP stack.
  eXosip_lock();
  i = eXosip_call_terminate (sipcall->getCid(), sipcall->getDid());
  eXosip_unlock();

  // Release RTP channels
  if (id == Manager::instance().getCurrentCallId()) {
    _audiortp.closeRtpSession();
  }

  deleteSipCall(id);
  return i;
}

int
SipVoIPLink::cancel (CALLID id) 
{
  int i = 0;
  SipCall* sipcall = getSipCall(id);
  _debug("%10d: Cancel call [cid = %d]\n", id, sipcall->getCid());
  // Release SIP stack.
  eXosip_lock();
  i = eXosip_call_terminate (sipcall->getCid(), -1);
  eXosip_unlock();

  deleteSipCall(id);
  return i;
}

/*
 * @return -1 = sipcall not present
 */
int
SipVoIPLink::onhold (CALLID id) 
{
  osip_message_t *invite;
  int i;
  int did;

  sdp_message_t *local_sdp = NULL;

  SipCall *sipcall = getSipCall(id);
  if ( sipcall == NULL ) { return -1; }

  did = sipcall->getDid();

  eXosip_lock ();
  local_sdp = eXosip_get_local_sdp (did);
  eXosip_unlock ();

  if (local_sdp == NULL) {
    return -1;
  }

  // Build INVITE_METHOD for put call on-hold
  eXosip_lock ();
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
  
  // Send request
  _audiortp.closeRtpSession();

  eXosip_lock ();
  i = eXosip_call_send_request (did, invite);
  eXosip_unlock ();
  
  // Disable audio
  return i;
}

/**
 * @return 0 is good, -1 is bad
 */
int
SipVoIPLink::offhold (CALLID id) 
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

  // Build INVITE_METHOD for put call off-hold
  eXosip_lock ();
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

  // Send request
  _debug("< Send off hold request\n");
  eXosip_lock ();
  i = eXosip_call_send_request (did, invite);
  eXosip_unlock ();

  // Enable audio
  _debug("          Starting AudioRTP\n");
  if (_audiortp.createNewSession (getSipCall(id)) < 0) {
    _debug("FATAL: Unable to start sound (%s:%d)\n", __FILE__, __LINE__);
    i = -1;
  }
  return i;
}

int
SipVoIPLink::transfer (CALLID id, const std::string& to)
{
  osip_message_t *refer;
  int i;
  std::string tmp_to;
  tmp_to = toHeader(to);
  if (tmp_to.find("@") == std::string::npos) {
    tmp_to = tmp_to + "@" + Manager::instance().getConfigString(SIGNALISATION,
HOST_PART);
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
SipVoIPLink::refuse (CALLID id)
{
  int i;
  char tmpbuf[64];
  bzero (tmpbuf, 64);
  // Get local port   
  snprintf (tmpbuf, 63, "%d", getSipCall(id)->getLocalAudioPort());
	
  osip_message_t *answerMessage = NULL;
  eXosip_lock();
  // not BUSY.. where decline the invitation!
  i = eXosip_call_build_answer (getSipCall(id)->getTid(), SIP_DECLINE, &answerMessage);
  if (i == 0) {
    i = eXosip_call_send_answer (getSipCall(id)->getTid(), SIP_DECLINE, answerMessage);
  }
  eXosip_unlock();
  return i;
}

int
SipVoIPLink::getEvent (void)
{
  // wait for 0 s, 50 ms
  eXosip_event_t* event = eXosip_event_wait (0, 50);
  eXosip_lock();
  eXosip_automatic_action();
  eXosip_unlock();

  if (event == NULL) {
    return -1;
  }

  SipCall* sipcall = NULL;
  CALLID id = 0;
  int returnValue = 0;

  _debug("Receive SipEvent #%d %s\n", event->type, event->textinfo);
  switch (event->type) {
    // IP-Phone user receives a new call
  case EXOSIP_CALL_INVITE: //
    _debug("> INVITE (receive)\n");
    checkNetwork();

    // Set local random port for incoming call
    if (!Manager::instance().useStun()) {
      setLocalPort(RANDOM_LOCAL_PORT);
    } else {
      // If there is a firewall
      if (behindNat() != 0) {
        setLocalPort(Manager::instance().getFirewallPort());
      } else {
        returnValue = -1;
        break;
      }
    }

    // Generate id
    id = Manager::instance().generateNewCallId();
    Manager::instance().pushBackNewCall(id, Incoming);
    _debug("%10d: [cid = %d, did = %d]\n", id, event->cid, event->did);

    // Associate an audio port with a call
    sipcall = getSipCall(id);
    sipcall->setLocalAudioPort(_localPort);
    sipcall->setLocalIp(getLocalIpAddress());
    _debug("            Local listening port: %d\n", _localPort);
    _debug("            Local listening IP: %s\n", getLocalIpAddress().c_str());

    if (sipcall->newIncomingCall(event) == 0 ) {
      if (Manager::instance().incomingCall(id, sipcall->getName(), sipcall->getNumber()) == -1) {
        Manager::instance().displayError("            Incoming Call Failed");
        deleteSipCall(id);
      }
    } else {
      Manager::instance().peerHungupCall(id);
      deleteSipCall(id);
      Manager::instance().displayError("            Incoming Call Failed");
    }
    break;

  case EXOSIP_CALL_REINVITE:
    _debug("> INVITE (reinvite)\n");
    //eXosip_call_send_answer(event->tid, 403, NULL);
    //488 as http://www.atosc.org/pipermail/public/osip/2005-June/005385.html

    id = findCallId(event);
    if (id != 0) {
      sipcall = getSipCall(id);
      if (sipcall != 0) {
        _debug("%10d: Receive Reinvite [cid = %d, did = %d], localport=%d\n", id, event->cid, event->did,sipcall->getLocalAudioPort());

        if ( id == Manager::instance().getCurrentCallId() ) {
          Manager::instance().stopTone();
          _audiortp.closeRtpSession();
        }
        sipcall->newReinviteCall(event);
        // we should receive an ack after that...
      }
    } else {
      _debug("< Send 488 Not Acceptable Here");
      eXosip_lock();
      eXosip_call_send_answer(event->tid, 488, NULL);
      eXosip_unlock();
    }
    break;

  case EXOSIP_CALL_PROCEEDING: // 8
    // proceeding call...
    break;

  case EXOSIP_CALL_RINGING: // 9 peer call is ringing
    id = findCallIdInitial(event);
    _debug("%10d: Receive Call Ringing [cid = %d, did = %d]\n", id, event->cid, event->did);
    if (id != 0) {
      getSipCall(id)->ringingCall(event);
      Manager::instance().peerRingingCall(id);
    } else {
      returnValue = -1;
    }
    break;

  // The peer-user answers
  case EXOSIP_CALL_ANSWERED: // 10
  {
    id = findCallIdInitial(event);
    if ( id != 0) {
      sipcall = getSipCall(id);
      if ( sipcall != 0 ) {
        _debug("%10d: Receive Call Answer [cid = %d, did = %d], localport=%d\n", id, event->cid, event->did, sipcall->getLocalAudioPort());

        // Answer
        if (Manager::instance().callCanBeAnswered(id)) {
          sipcall->setStandBy(false);
          if (sipcall->answeredCall(event) != -1) {
            sipcall->answeredCall_without_hold(event);
            Manager::instance().peerAnsweredCall(id);
  
            if(!Manager::instance().callIsOnHold(id) && Manager::instance().getCurrentCallId()==id) {
              // Outgoing call is answered, start the sound channel.
              _debug("            Starting AudioRTP\n");
              if (_audiortp.createNewSession(sipcall) < 0) {
                _debug("            FATAL: Unable to start sound (%s:%d)\n", 
                __FILE__, __LINE__);
                returnValue = -1;
              }
            }
          }
        } else {
          // Answer to on/off hold to send ACK
          _debug("            Answering call\n");
          sipcall->answeredCall(event);
        }
      }
    } else {
      returnValue = -1;
    }
  }
   break;
  case EXOSIP_CALL_REDIRECTED: // 11
    break;

  case EXOSIP_CALL_ACK: // 15
    id = findCallId(event); 
    _debug("%10d: Receive ACK [cid = %d, did = %d]\n", id, event->cid, event->did);
    if (id != 0 ) {
      sipcall = getSipCall(id);
      if(sipcall != 0 ) { 
        sipcall->receivedAck(event);
        if (sipcall->isReinvite()) {
          sipcall->endReinvite();
          if(!Manager::instance().callIsOnHold(id) && Manager::instance().getCurrentCallId()==id) {
            _debug("            Starting AudioRTP\n");
            _audiortp.createNewSession(sipcall);
          } else {
            _debug("            Didn't start RTP because it's on hold or it's not the current call id\n");
          }
        }
      }
    } else {
      returnValue = -1;
    }
    break;

    // The peer-user closed the phone call(we received BYE).
  case EXOSIP_CALL_CLOSED: // 25
    id = findCallId(event);
    if (id==0) { id = findCallIdInitial(event); }
    _debug("%10d: Receive BYE [cid = %d, did = %d]\n", id, event->cid, event->did);	
    if (id != 0) {
      if (Manager::instance().callCanBeClosed(id)) {
         sipcall = getSipCall(id);
         _audiortp.closeRtpSession();
      }
      Manager::instance().peerHungupCall(id);
      deleteSipCall(id);
    } else {
      returnValue = -1;
    }	
    break;
  case EXOSIP_CALL_RELEASED:
    if (event) {
      _debug("SIP call released: [cid = %d, did = %d]\n", event->cid, event->did);
      id = findCallId(event);
      if (id!=0) {
        // not supposed to be execute on a current call...
        Manager::instance().callFailure(id);
        deleteSipCall(id);
      }

    }
    break;
  case EXOSIP_CALL_REQUESTFAILURE:
    id = findCallId(event);

    // Handle 4XX errors
    switch (event->response->status_code) {
    case AUTH_REQUIRED:
      _debug("SIP Server ask required authentification: logging...\n");
      setAuthentication();
      eXosip_lock();
      eXosip_automatic_action();
      eXosip_unlock();
      break;
    case UNAUTHORIZED:
      _debug("Request is unauthorized. SIP Server ask authentification: logging...\n");
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
    case NOT_ACCEPTABLE_HERE: // 488
      // Display error on the screen phone
      //Manager::instance().displayError(event->response->reason_phrase);
      Manager::instance().displayErrorText(id, event->response->reason_phrase);
      Manager::instance().callFailure(id);
      deleteSipCall(id);
    break;
    case BUSY_HERE:
      Manager::instance().displayErrorText(id, event->response->reason_phrase);
      Manager::instance().callBusy(id);
      deleteSipCall(id);
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
      id = findCallId(event);
      Manager::instance().callFailure(id);
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
      id = findCallId(event);
      Manager::instance().callFailure(id);
      break;
    default:
      break;
    }
    break;

  case EXOSIP_CALL_MESSAGE_NEW: // 18
    if (0 == event->request) break;
    if (MSG_IS_INFO(event->request)) {
      _debug("Receive a call message request info\n");
      osip_content_type_t* c_t = event->request->content_type;
      if (c_t != 0 && c_t->type != 0 && c_t->subtype != 0 ) {
        _debug("  Content Type of the message: %s/%s\n", c_t->type, c_t->subtype);
        // application/dtmf-relay
        if (strcmp(c_t->type, "application") == 0 && strcmp(c_t->subtype, "dtmf-relay") == 0) {
          handleDtmfRelay(event);
        }
      }
    }

    osip_message_t *answerOKNewMessage;
    eXosip_lock();
    if ( 0 == eXosip_call_build_answer(event->tid, OK, &answerOKNewMessage)) {
      _debug("< Sending 200 OK\n");
      eXosip_call_send_answer(event->tid, OK, answerOKNewMessage);
    } else {
      _debug("Could not sent an OK message\n");
    }
    eXosip_unlock();
    break;
 
  case EXOSIP_REGISTRATION_SUCCESS: // 1
    // Manager::instance().displayStatus(LOGGED_IN_STATUS);
    Manager::instance().registrationSucceed();
    break;

  case EXOSIP_REGISTRATION_FAILURE: // 2
    //Manager::instance().displayError("getEvent : Registration Failure");
    Manager::instance().registrationFailed();
    break;

  case EXOSIP_MESSAGE_NEW: //27

    if ( event->request == NULL) {
      break; // do nothing
    }
    unsigned int k;
				
    if (MSG_IS_OPTIONS(event->request)) {
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
    else if (MSG_IS_NOTIFY(event->request)){
      _debug("> NOTIFY Voice message\n");
      int ii;
      unsigned int pos;
      unsigned int pos_slash;

      osip_body_t *body = NULL;
      // Get the message body
      ii = osip_message_get_body(event->request, 0, &body);
      if (ii != 0) {
        _debug("  Cannot get body in a new EXOSIP_MESSAGE_NEW event\n");
        returnValue = -1;
        break;
      }

      // Analyse message body
      if (!body || !body->body) {
        returnValue = -1;
        break;
      }
      std::string str(body->body);
      pos = str.find(VOICE_MSG);

      if (pos == std::string::npos) {
	     // If the string is not found
        returnValue = -1;
        break;
      } 

      pos_slash = str.find ("/");
      std::string nb_msg = str.substr(pos + LENGTH_VOICE_MSG, 
      pos_slash - (pos + LENGTH_VOICE_MSG));

      // Set the number of voice-message
      setMsgVoicemail(atoi(nb_msg.data()));

      if (getMsgVoicemail() != 0) {
        // If there is at least one voice-message, start notification
        Manager::instance().startVoiceMessageNotification(nb_msg);
      } else {
        // Stop notification when there is 0 voice message
        Manager::instance().stopVoiceMessageNotification();
      }
    // http://www.jdrosen.net/papers/draft-ietf-simple-im-session-00.txt
    } else if (MSG_IS_MESSAGE(event->request)) {
      _debug("> MESSAGE received\n");
      // osip_content_type_t* osip_message::content_type
      osip_content_type_t* c_t = event->request->content_type;
      if (c_t != 0 &&  c_t->type != 0 && c_t->subtype != 0 ) {
        _debug("  Content Type of the message: %s/%s\n", c_t->type, c_t->subtype);

        osip_body_t *body = NULL;
        // Get the message body
        if (0 == osip_message_get_body(event->request, 0, &body)) {
          _debug("  Body length: %d\n", body->length);
          if (body->body!=0 && 
              strcmp(c_t->type,"text") == 0 && 
              strcmp(c_t->subtype,"plain") == 0
            ) {
            _debug("  Text body: %s\n", body->body);
            Manager::instance().incomingMessage(body->body);
          }
        }
      }
      osip_message_t *answerOK;
      eXosip_lock();
      if ( 0 == eXosip_message_build_answer(event->tid, OK, &answerOK)) {
          _debug("< Sending 200 OK\n");
          eXosip_message_send_answer(event->tid, OK, answerOK);
      }
      eXosip_unlock();
    }
    break;

  case EXOSIP_SUBSCRIPTION_ANSWERED: // 38
    eXosip_lock();
    eXosip_automatic_action();
    eXosip_unlock();
    break;

  case EXOSIP_SUBSCRIPTION_REQUESTFAILURE: //40
    break;

  default:
    returnValue = -1;
    break;
  }
  eXosip_event_free(event);

  return returnValue;
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
SipVoIPLink::carryingDTMFdigits (CALLID id, char code) {
  SipCall* sipcall = getSipCall(id);
  if (sipcall == 0) { return; }

  int duration = Manager::instance().getConfigInt(SIGNALISATION, PULSE_LENGTH);
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
    i = eXosip_call_send_request(sipcall->getDid(), info);
  }
  eXosip_unlock();
	
  delete[] dtmf_body; dtmf_body = NULL;
}
 
void
SipVoIPLink::newOutgoingCall (CALLID id)
{
  SipCall* sipcall = new SipCall(id, Manager::instance().getCodecDescVector());
  if (sipcall != NULL) {
    _sipcallVector.push_back(sipcall);
    sipcall->setStandBy(true);
  }
}

void
SipVoIPLink::newIncomingCall (CALLID id)
{
  SipCall* sipcall = new SipCall(id, Manager::instance().getCodecDescVector());
  if (sipcall != NULL) {
    _sipcallVector.push_back(sipcall);
  }
}

void
SipVoIPLink::deleteSipCall (CALLID id)
{
  std::vector< SipCall* >::iterator iter = _sipcallVector.begin();

  while(iter != _sipcallVector.end()) {
    if (*iter && (*iter)->getId() == id) {
      delete *iter; *iter = NULL;
      _sipcallVector.erase(iter);
      return;
    }
    iter++;
  }
}

void
SipVoIPLink::endSipCalls()
{
  std::vector< SipCall* >::iterator iter = _sipcallVector.begin();
  while(iter != _sipcallVector.end()) {
    if ( *iter ) {

      // Release SIP stack.
      eXosip_lock();
      eXosip_call_terminate ((*iter)->getCid(), (*iter)->getDid());
      eXosip_unlock();

      // Release RTP channels
      _audiortp.closeRtpSession();
      delete *iter; *iter = NULL;
    }
    iter++;
  }
  _sipcallVector.clear();
}

SipCall*
SipVoIPLink::getSipCall (CALLID id)
{
  SipCall* sipcall = NULL;
  for (unsigned int i = 0; i < _sipcallVector.size(); i++) {
    sipcall = _sipcallVector.at(i);
    if (sipcall && sipcall->getId() == id) {
      return sipcall;
    } 
  }
  return NULL;
}

AudioCodec*
SipVoIPLink::getAudioCodec (CALLID id)
{
  SipCall* sipcall = getSipCall(id);
  if (sipcall != NULL) {
    return sipcall->getAudioCodec();
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
SipVoIPLink::sdp_complete_200ok (int did, osip_message_t * answerMessage, int port)
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

  // thus exosip call is protected by an extern lock
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

  osip_message_set_body (answerMessage, buf, strlen (buf));
  osip_message_set_content_type (answerMessage, "application/sdp");
  sdp_message_free (remote_sdp);
  return 0;
}


int
SipVoIPLink::behindNat (void)
{
  StunAddress4 stunSvrAddr;
  stunSvrAddr.addr = 0;
	
  // Stun server
  std::string svr = Manager::instance().getConfigString(SIGNALISATION, STUN_SERVER);
	
  // Convert char* to StunAddress4 structure
  bool ret = stunParseServerName ((char*)svr.data(), stunSvrAddr);
  if (!ret) {
    _debug("SIP: Stun server address (%s) is not valid\n", svr.data());
    return 0;
  }
	
  // Firewall address
  //_debug("STUN server: %s\n", svr.data());
  Manager::instance().getStunInfo(stunSvrAddr);

  return 1;
}

/**
 * Get the local Ip by eXosip 
 * only if the local ip address is to his default value: 127.0.0.1
 * setLocalIpAdress
 * @return false if not found
 */
bool
SipVoIPLink::getSipLocalIp (void) 
{
  bool returnValue = true;
  if (getLocalIpAddress() == "127.0.0.1") {
    char* myIPAddress = new char[65];
    if (eXosip_guess_localip(AF_INET, myIPAddress, 64) == -1) {
      returnValue = false;
    } else {
      setLocalIpAddress(std::string(myIPAddress));
      _debug("Checking network, setting local ip address to: %s\n", myIPAddress);
    }
    delete [] myIPAddress; myIPAddress = NULL;
  }
  return returnValue;
}

int
SipVoIPLink::checkUrl (const std::string& url)
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
  ManagerImpl& manager = Manager::instance();
  std::string login = manager.getConfigString(SIGNALISATION, AUTH_USER_NAME);
  if (login.empty()) {
    login = manager.getConfigString(SIGNALISATION, USER_PART);
  }
  std::string pass = manager.getConfigString(SIGNALISATION, PASSWORD);
  if (pass.empty()) {
    manager.displayConfigError("Fill password field");
    return -1;
  }
  int returnValue = 0;
  eXosip_lock();
  if (eXosip_add_authentication_info(login.data(), login.data(), pass.data(), NULL, NULL) != 0) {
    returnValue = -1;
  }
  eXosip_unlock();
  return returnValue;
}

std::string
SipVoIPLink::fromHeader (const std::string& user, const std::string& host) 
{
  std::string displayname = Manager::instance().getConfigString(SIGNALISATION,
FULL_NAME);
  return ("\"" + displayname + "\"" + " <sip:" + user + "@" + host + ">");
}


std::string
SipVoIPLink::toHeader(const std::string& to) 
{
  if (to.find("sip:") == std::string::npos) {
    return ("sip:" + to );
  } else {
    return to;
  }
}

int
SipVoIPLink::startCall (CALLID id, const std::string& from, const std::string& to, const std::string& subject, const std::string& route) 
{
  SipCall* sipcall = getSipCall(id);
  if ( sipcall == NULL) {
    return -1; // error, we can't find the sipcall
  }
  osip_message_t *invite;

  if (checkUrl(from) != 0) {
    Manager::instance().displayConfigError("Error in source address");
    return -1;
  }
  if (checkUrl(to) != 0) {
    Manager::instance().displayErrorText(id, "Error in destination address");
    return -1;
  }
	
  if (!Manager::instance().useStun()) {
    // Set random port for outgoing call if no firewall
    setLocalPort(RANDOM_LOCAL_PORT);
    _debug("            Setting local port to random: %d\n",_localPort);
  } else {
    // If use Stun server
    if (behindNat() != 0) {
      setLocalPort(Manager::instance().getFirewallPort());
      _debug("            Setting local port to firewall port: %d\n", _localPort);	
    } else {
      return -1;
    }
  }
	
  // Set local audio port for sipcall(id)
  sipcall->setLocalAudioPort(_localPort);
  sipcall->setLocalIp(getLocalIpAddress());

  eXosip_lock();
  int i = eXosip_call_build_initial_invite (&invite, (char*)to.data(),
                                        (char*)from.data(),
                                        (char*)route.data(),
                                        (char*)subject.data());
  
  if (i != 0) {
    eXosip_unlock();
    return -1; // error when building the invite
  }

  int payload;
  char rtpmap[128];
  char rtpmap_attr[2048];
  char media[64];
  char media_audio[64];

  bzero(rtpmap, 128);
  bzero(rtpmap_attr, 2048);
  bzero(media, 64);
  bzero(media_audio, 64);

  // Set rtpmap according to the supported codec order
  CodecDescriptorVector* cdv = Manager::instance().getCodecDescVector();
  unsigned int nb = cdv->size();
  for (unsigned int iCodec = 0; iCodec < nb; iCodec++) {
    payload = cdv->at(iCodec)->getPayload();

    // Add payload to rtpmap if it is not already added
    if (!isInRtpmap(iCodec, payload, cdv)) {
      snprintf(media, 63, "%d ", payload);
      strcat (media_audio, media);

      snprintf(rtpmap, 127, "a=rtpmap: %d %s/%d\r\n", payload, 
	       cdv->at(iCodec)->rtpmapPayload(payload).data(), SAMPLING_RATE);
      strcat(rtpmap_attr, rtpmap);
    }
  }

  // http://www.antisip.com/documentation/eXosip2/group__howto1__initialize.html
  // tell sip if we support SIP extension like 100rel
  // osip_message_set_supported (invite, "100rel");

  /* add sdp body */
  {
    char tmp[4096];
    snprintf (tmp, 4096,
              "v=0\r\n"
              "o=SFLphone 0 0 IN IP4 %s\r\n"
              "s=call\r\n"
              "c=IN IP4 %s\r\n"
              "t=0 0\r\n"
              "m=audio %d RTP/AVP %s\r\n"
	            "%s",
              getLocalIpAddress().c_str(), getLocalIpAddress().c_str(), getLocalPort(), media_audio, rtpmap_attr);
    // media_audio should be one, two or three numbers?
    osip_message_set_body (invite, tmp, strlen (tmp));
    osip_message_set_content_type (invite, "application/sdp");
  }
  
  // this is the cid (call id from exosip)
  _debug("%10d: Receive INVITE\n", id);
  int cid = eXosip_call_send_initial_invite (invite);
  _debug("            Local IP:port: %s:%d\n", getLocalIpAddress().c_str(), getLocalPort());
  _debug("            Payload:       %s\n", media_audio);

  // Keep the cid in case of cancelling
  sipcall->setCid(cid);

  if (cid <= 0) {
    eXosip_unlock();
    return -1;
  } else {
    eXosip_call_set_reference (cid, NULL);
  }
  eXosip_unlock();

  return cid; // this is the Cid
}

CALLID
SipVoIPLink::findCallId (eXosip_event_t *e)
{
  for (unsigned int k = 0; k < _sipcallVector.size(); k++) {
    SipCall* sipcall = _sipcallVector.at(k);
    if (sipcall && sipcall->getCid() == e->cid &&
        sipcall->getDid() == e->did) {
      return sipcall->getId();
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
CALLID
SipVoIPLink::findCallIdInitial (eXosip_event_t *e)
{
  for (unsigned int k = 0; k < _sipcallVector.size(); k++) {
    SipCall* sipcall = _sipcallVector.at(k);
    // the dialog id is not set when you do a new call
    // so you can't check it when you want to retreive it
    // for the first call anwser
    if (sipcall && sipcall->getCid() == e->cid) {
      return sipcall->getId();
    }
  }
  return 0;
}

/**
 * Handle an INFO with application/dtmf-relay content-type
 * @param event eXosip Event
 */
bool
SipVoIPLink::handleDtmfRelay(eXosip_event_t* event) {
  bool returnValue = false;
  osip_body_t *body = NULL;
  // Get the message body
  if (0 == osip_message_get_body(event->request, 0, &body) && body->body != 0 )   {
    _debug("  Text body: %s\n", body->body);
    std::string dtmfBody(body->body);
    unsigned int posStart = 0;
    unsigned int posEnd = 0;
    std::string signal;
    std::string duration;
    // search for signal=and duration=
    posStart = dtmfBody.find("Signal=");
    if (posStart != std::string::npos) {
      posStart += strlen("Signal=");
      posEnd = dtmfBody.find("\n", posStart);
      if (posEnd == std::string::npos) {
        posEnd = dtmfBody.length();
      }
      signal = dtmfBody.substr(posStart, posEnd-posStart+1);
      _debug("Signal value: %s\n", signal.c_str());
      
      if (!signal.empty()) {
        unsigned int id = findCallId(event);
        if (id !=0 && id == Manager::instance().getCurrentCallId()) {
          Manager::instance().playDtmf(signal[0]);
        }
      }
/*
 // we receive the duration, but we use our configuration...

      posStart = dtmfBody.find("Duration=");
      if (posStart != std::string::npos) {
        posStart += strlen("Duration=");
        posEnd = dtmfBody.find("\n", posStart);
        if (posEnd == std::string::npos) {
            posEnd = dtmfBody.length();
        }
        duration = dtmfBody.substr(posStart, posEnd-posStart+1);
        _debug("Duration value: %s\n", duration.c_str());
        returnValue = true;
      }
*/
    }
  }
  return returnValue;
}

