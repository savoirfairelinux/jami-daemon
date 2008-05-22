/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author : Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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
/*
 * YM: 2006-11-15: changes unsigned int to std::string::size_type, thanks to Pierre Pomes (AMD64 compilation)
 */
#include "sipvoiplink.h"
#include "eventthread.h"
#include "sipcall.h"
#include <sstream> // for ostringstream
#include "sipaccount.h"

#include "manager.h"
#include "user_cfg.h" // SIGNALISATION / PULSE #define

// for listener
#define DEFAULT_SIP_PORT  5060
#define RANDOM_SIP_PORT   rand() % 64000 + 1024
#define RANDOM_LOCAL_PORT ((rand() % 27250) + 5250)*2

#define EXOSIP_ERROR_NO   0
#define EXOSIP_ERROR_STD -1
#define EXOSIP_ERROR_BUILDING -2

// for registration
//#define EXPIRES_VALUE 180

// 1XX responses
#define DIALOG_ESTABLISHED 101
// see: osip_const.h

// FOR VOICE Message handling
#define VOICE_MSG     "Voice-Message"
#define LENGTH_VOICE_MSG  15

// need for hold/unhold
#define INVITE_METHOD "INVITE"


SIPVoIPLink::SIPVoIPLink(const AccountID& accountID)
 : VoIPLink(accountID), _localExternAddress("") , eXosip_running( false )
{
  _evThread = new EventThread(this);


  _nMsgVoicemail = 0;
  _eXosipRegID = EXOSIP_ERROR_STD;

  _nbTryListenAddr = 2; // number of times to try to start SIP listener
  _localExternPort = 0;

  // to get random number for RANDOM_PORT
  srand (time(NULL));
}

SIPVoIPLink::~SIPVoIPLink()
{
  terminate();
  delete _evThread; _evThread = 0;
}

bool 
SIPVoIPLink::init()
{
  if( eXosip_running ){
    delete _evThread; 
    _evThread=0;
    _evThread=  new EventThread( this );
    eXosip_quit();
  }

  if (!_initDone) {
    if (0 != eXosip_init()) {
      _debug("! SIP Failure: Could not initialize eXosip\n");
      return false;
    }

    // Pour éviter qu'on refasse l'init sans avoir considéré l'erreur,
    // s'il y en a une ?
    _initDone = true;
    // check networking capabilities
    if ( !checkNetwork() ) {
      _debug("! SIP FAILURE: Unable to determine network capabilities\n");
      return false;
    }
  
    // if we useStun and we failed to receive something on port 5060, we try a random port
    // If use STUN server, firewall address setup
    int errExosip = 0;
    int port = DEFAULT_SIP_PORT;
  
    int iTry = 1;  // try number..
  
    do {
      if (_useStun && !Manager::instance().behindNat(_stunServer, port)) { 
        port = RANDOM_SIP_PORT; 
        if (!Manager::instance().behindNat(_stunServer, port)) {
         _debug("! SIP Failure: Unable to check NAT setting\n");
          return false; // hoho we can't use the random sip port too...
        }
      }
  
      // second parameter, NULL is "::" for ipv6 and "0.0.0.0" for ipv4, we can put INADDR_ANY
      errExosip = eXosip_listen_addr(IPPROTO_UDP, INADDR_ANY, port, AF_INET, 0);
      if (errExosip != 0) {
        _debug("* SIP Info: [%d/%d] could not initialize SIP listener on port %d\n", iTry, _nbTryListenAddr, port);
        port = RANDOM_SIP_PORT;
      }
    } while ( errExosip != 0 && iTry < _nbTryListenAddr );
  
    if ( errExosip != 0 ) { // we didn't succeeded
      _debug("! SIP Failure: SIP failed to listen on port %d\n", port);
      return false;
    }
    _localPort = port;
    _debug("  SIP Init: listening on port %d\n", port);
    
    if (_useStun) {
      // This method is used to replace contact address with the public address of your NAT
      // it should be call after eXosip_listen_addr
      // set by last behindNat() call (ish)...
      _localExternAddress  = Manager::instance().getFirewallAddress();
      _localExternPort     = Manager::instance().getFirewallPort();
      eXosip_masquerade_contact(_localExternAddress.data(), _localExternPort);
    } else {
      _localExternAddress = _localIPAddress;
      _localExternPort    = _localPort;
    }
    
    // Set user agent
    std::string tmp = std::string(PROGNAME_GLOBAL) + "/" + std::string(SFLPHONED_VERSION);
    eXosip_set_user_agent(tmp.data());
  
    _debug(" SIP Init: starting loop thread (SIP events)\n" );
    _evThread->start();
  }

  _initDone = true;
  eXosip_running = true;
  // Useless
  return true;
}

void 
SIPVoIPLink::terminate()
{
  terminateSIPCall(); 
    // TODO The next line makes the daemon crash on 
    // account delete if at least one account is registered.
    // It should called only when the last account 
    // is deleted/unregistered.
    _initDone = false;
}

void
SIPVoIPLink::terminateSIPCall()
{
  
  ost::MutexLock m(_callMapMutex);
  CallMap::iterator iter = _callMap.begin();
  SIPCall *call;
  while( iter != _callMap.end() ) {
    call = dynamic_cast<SIPCall*>(iter->second);
    if (call) {
      // Release SIP stack.
      eXosip_lock();
      eXosip_call_terminate(call->getCid(), call->getDid() );
      eXosip_unlock();
      delete call; call = 0;
    }
    iter++;
  }
  _callMap.clear();
}

bool
SIPVoIPLink::checkNetwork()
{
  // Set IP address
  return loadSIPLocalIP();
}

bool
SIPVoIPLink::loadSIPLocalIP() 
{
  bool returnValue = true;
  if (_localIPAddress == "127.0.0.1") {
    char* myIPAddress = new char[65];
    if (eXosip_guess_localip(AF_INET, myIPAddress, 64) == EXOSIP_ERROR_STD) {
      // Update the registration state if no network capabilities found
      setRegistrationState( ErrorNetwork );
      returnValue = false;
    } else {
      _localIPAddress = std::string(myIPAddress);
      _debug("  SIP Info: Checking network, setting local IP address to: %s\n", myIPAddress);
    }
    delete [] myIPAddress; myIPAddress = NULL;
  }
  return returnValue;
}

void
SIPVoIPLink::getEvent()
{
	char* tmp2;
	eXosip_event_t* event = eXosip_event_wait(0, 50);
	eXosip_lock();
	eXosip_automatic_action();
	eXosip_unlock();

	if ( event == NULL ) {
		return;
	}

      
	_debug("> SIP Event: [cdt=%4d:%4d:%4d] type=#%03d %s \n", event->cid, event->did, event->tid, event->type, event->textinfo);
	switch (event->type) {
	/* REGISTER related events */
	case EXOSIP_REGISTRATION_NEW:         /** 00 < announce new registration.       */
		_debugMid(" !EXOSIP_REGISTRATION_NEW event is not implemented\n");
		break;
	case EXOSIP_REGISTRATION_SUCCESS:     /** 01 < user is successfully registred.  */
		_debugMid(" !EXOSIP_REGISTRATION_SUCCESS ---> %s\n" , getAccountID().c_str());
		if(_eXosipRegID == EXOSIP_ERROR_STD){
		  _debug("Successfully Unregister account ID = %s\n" , getAccountID().c_str());
		  setRegistrationState(Unregistered);
		}
		else{
		  _debug("Successfully Register account ID = %s\n" , getAccountID().c_str());
		  setRegistrationState(Registered);
		}
		break;
	case EXOSIP_REGISTRATION_FAILURE:     /** 02 < user is not registred.           */
		SIPRegistrationFailure( event );
		_debugMid(" !EXOSIP_REGISTRATION_FAILURE\n");
		break;
	case EXOSIP_REGISTRATION_REFRESHED:   /** 03 < registration has been refreshed. */
		_debugMid(" !EXOSIP_REGISTRATION_REFRESHED event is not implemented\n");
		break;
	case EXOSIP_REGISTRATION_TERMINATED:  /** 04 < UA is not registred any more.    */
		//setRegistrationState(Unregistered, "Registration terminated by remote host");
		setRegistrationState(Unregistered);
		_debugMid(" !EXOSIP_REGISTRATION_TERMINATED event is not implemented\n");
		break;

	/* INVITE related events within calls */
	case EXOSIP_CALL_INVITE:          /** 05 < announce a new call                   */
		_debugMid(" !EXOSIP_CALL_INVITE\n");
		SIPCallInvite(event);
		break;
	case EXOSIP_CALL_REINVITE:        /** 06 < announce a new INVITE within call     */
		SIPCallReinvite(event);
		_debugMid(" !EXOSIP_REGISTRATION_TERMINATED event is not implemented\n");
		break;

	/* CALL related events */
	case EXOSIP_CALL_NOANSWER:        /** 07 < announce no answer within the timeout */
		_debugMid(" !EXOSIP_CALL_NOANSWER event is not implemented\n");
		break;
	case EXOSIP_CALL_PROCEEDING:      /** 08 < announce processing by a remote app   */
		_debugMid(" !EXOSIP_CALL_PROCEEDING event is not implemented\n");
		break;
	case EXOSIP_CALL_RINGING:         /** 09 < announce ringback                     */
		_debugMid(" !EXOSIP_CALL_RINGING\n");
		SIPCallRinging(event);
		break;
	case EXOSIP_CALL_ANSWERED:        /** 10 < announce start of call                */
		_debugMid(" !EXOSIP_CALL_ANSWERED\n");
		SIPCallAnswered(event);
		break;
	case EXOSIP_CALL_REDIRECTED:      /** 11 < announce a redirection                */
		_debugMid(" !EXOSIP_CALL_REDIRECTED event is not implemented\n");
		break;
	case EXOSIP_CALL_REQUESTFAILURE:  /** 12 < announce a request failure            */
		_debugMid(" !EXOSIP_CALL_REQUESTFAILURE");
		SIPCallRequestFailure(event);
		break;
	case EXOSIP_CALL_SERVERFAILURE:   /** 13 < announce a server failure             */
		_debugMid(" !EXOSIP_CALL_SERVERFAILURE");
		SIPCallServerFailure(event);
		break;
	case EXOSIP_CALL_GLOBALFAILURE:   /** 14 < announce a global failure             */
		_debugMid(" !EXOSIP_CALL_GLOBALFAILURE\n");
		SIPCallServerFailure(event);
		break;
	case EXOSIP_CALL_ACK:             /** 15 < ACK received for 200ok to INVITE      */
		_debugMid(" !EXOSIP_CALL_ACK\n");
		SIPCallAck(event);
		break;
	case EXOSIP_CALL_CANCELLED:       /** 16 < announce that call has been cancelled */
		_debugMid(" !EXOSIP_CALL_CANCELLED\n");
		break;
	case EXOSIP_CALL_TIMEOUT:         /** 17 < announce that call has failed         */
		_debugMid(" !EXOSIP_CALL_TIMEOUT\n");
		break;

	/* Request related events within calls (except INVITE) */
	case EXOSIP_CALL_MESSAGE_NEW:            /** 18 < announce new incoming MESSAGE. */
		_debugMid(" !EXOSIP_CALL_MESSAGE_NEW\n");
		SIPCallMessageNew(event);
		break;
	case EXOSIP_CALL_MESSAGE_PROCEEDING:     /** 19 < announce a 1xx for MESSAGE. */
		_debugMid(" !EXOSIP_CALL_MESSAGE_PROCEEDING\n");
		break;
	case EXOSIP_CALL_MESSAGE_ANSWERED:       /** 20 < announce a 200ok  */
		// 200 OK
		_debugMid(" !EXOSIP_CALL_MESSAGE_ANSWERED\n");
		break;
	case EXOSIP_CALL_MESSAGE_REDIRECTED:     /** 21 < announce a failure. */
		_debugMid(" !EXOSIP_CALL_MESSAGE_REDIRECTED\n");
		break;
	case EXOSIP_CALL_MESSAGE_REQUESTFAILURE: /** 22 < announce a failure. */
		_debugMid(" !EXOSIP_CALL_MESSAGE_REQUESTFAILURE\n");
		break;
	case EXOSIP_CALL_MESSAGE_SERVERFAILURE:  /** 23 < announce a failure. */
		_debugMid(" !EXOSIP_CALL_MESSAGE_SERVERFAILURE\n");
		break;
	case EXOSIP_CALL_MESSAGE_GLOBALFAILURE:  /** 24 < announce a failure. */
		_debugMid(" !EXOSIP_CALL_MESSAGE_GLOBALFAILURE\n");
		break;

	case EXOSIP_CALL_CLOSED:          /** 25 < a BYE was received for this call */
		_debugMid(" !EXOSIP_CALL_CLOSED\n");
		SIPCallClosed(event);
		break;

	/* For both UAS & UAC events */
	case EXOSIP_CALL_RELEASED:           /** 26 < call context is cleared. */
		_debugMid(" !EXOSIP_CALL_RELEASED\n");
		SIPCallReleased(event);
		break;

	/* Response received for request outside calls */
	case EXOSIP_MESSAGE_NEW:            /** 27 < announce new incoming MESSAGE. */
		_debugMid(" !EXOSIP_MESSAGE_NEW\n");
		if (event->request == NULL) { break; }
		SIPMessageNew(event);
		break;
	case EXOSIP_MESSAGE_PROCEEDING:     /** 28 < announce a 1xx for MESSAGE. */
		_debugMid(" !EXOSIP_MESSAGE_PROCEEDING\n");
		break;
	case EXOSIP_MESSAGE_ANSWERED:       /** 29 < announce a 200ok  */
		_debugMid(" !EXOSIP_MESSAGE_ANSWERED\n");
		break;
	case EXOSIP_MESSAGE_REDIRECTED:     /** 30 < announce a failure. */
		_debugMid(" !EXOSIP_MESSAGE_REDIRECTED\n");
		break;

	case EXOSIP_MESSAGE_REQUESTFAILURE: /** 31 < announce a failure. */
		_debugMid(" !EXOSIP_MESSAGE_REQUESTFAILURE\n");
		if (event->response !=0 && event->response->status_code == SIP_METHOD_NOT_ALLOWED) 
			Manager::instance().incomingMessage(getAccountID(), "Message are not allowed");
		break;
	case EXOSIP_MESSAGE_SERVERFAILURE:  /** 32 < announce a failure. */
		_debugMid(" !EXOSIP_MESSAGE_SERVERFAILURE\n");
		break;
	case EXOSIP_MESSAGE_GLOBALFAILURE:  /** 33 < announce a failure. */
		_debugMid(" !EXOSIP_MESSAGE_GLOBALFAILURE\n");
		break;

	/* Presence and Instant Messaging */
	case EXOSIP_SUBSCRIPTION_UPDATE:       /** 34 < announce incoming SUBSCRIBE.      */
		_debugMid(" !EXOSIP_SUBSCRIPTION_UPDATE\n");
		break;
	case EXOSIP_SUBSCRIPTION_CLOSED:       /** 35 < announce end of subscription.     */
		_debugMid(" !EXOSIP_SUBSCRIPTION_CLOSED\n");
		break;

	case EXOSIP_SUBSCRIPTION_NOANSWER:        /** 37 < announce no answer              */
		_debugMid(" !EXOSIP_SUBSCRIPTION_NOANSWER\n");
		break;
	case EXOSIP_SUBSCRIPTION_PROCEEDING:      /** 38 < announce a 1xx                  */
		_debugMid(" !EXOSIP_SUBSCRIPTION_PROCEEDING\n");
		break;
	case EXOSIP_SUBSCRIPTION_ANSWERED:        /** 39 < announce a 200ok                */
		_debugMid(" !EXOSIP_SUBSCRIPTION_ANSWERED\n");
		eXosip_lock();
		eXosip_automatic_action();
		eXosip_unlock();
		break;

	case EXOSIP_SUBSCRIPTION_REDIRECTED:      /** 40 < announce a redirection          */
		_debugMid(" !EXOSIP_SUBSCRIPTION_REDIRECTED\n");
		break;
	case EXOSIP_SUBSCRIPTION_REQUESTFAILURE:  /** 41 < announce a request failure      */
		_debugMid(" !EXOSIP_SUBSCRIPTION_REQUESTFAILURE\n");
		break;
	case EXOSIP_SUBSCRIPTION_SERVERFAILURE:   /** 42 < announce a server failure       */
		_debugMid(" !EXOSIP_SUBSCRIPTION_REQUESTFAILURE\n");
		break;
	case EXOSIP_SUBSCRIPTION_GLOBALFAILURE:   /** 43 < announce a global failure       */
		_debugMid(" !EXOSIP_SUBSCRIPTION_GLOBALFAILURE\n");
		break;
	case EXOSIP_SUBSCRIPTION_NOTIFY:          /** 44 < announce new NOTIFY request     */
		_debugMid(" !EXOSIP_SUBSCRIPTION_NOTIFY\n");
		osip_body_t* body;
		osip_from_to_str(event->request->from, &tmp2);
		osip_message_get_body(event->request, 0, &body);
		if (body != NULL && body->body != NULL) {
//			printf("\n---------------------------------\n");
//			printf ("(%i) from: %s\n  %s\n", event->tid, tmp2, body->body);
//			printf("---------------------------------\n");
		}
		osip_free(tmp2);
		break;
	case EXOSIP_SUBSCRIPTION_RELEASED:        /** 45 < call context is cleared.        */
		_debugMid(" !EXOSIP_SUBSCRIPTION_RELEASED\n");
		break;

	case EXOSIP_IN_SUBSCRIPTION_NEW:          /** 46 < announce new incoming SUBSCRIBE.*/
		_debugMid(" !EXOSIP_IN_SUBSCRIPTION_NEW\n");
		break;
	case EXOSIP_IN_SUBSCRIPTION_RELEASED:     /** 47 < announce end of subscription.   */
		_debugMid(" !EXOSIP_IN_SUBSCRIPTION_RELEASED\n");
		break;

	case EXOSIP_EVENT_COUNT:               /** 48 < MAX number of events  */
		_debugMid(" !EXOSIP_EVENT_COUNT : SHOULD NEVER HAPPEN!!!!!\n");
		break;
	default:
		printf("received eXosip event (type, did, cid) = (%d, %d, %d)", event->type, event->did, event->cid);
		break;
	}
	eXosip_event_free(event);
}

bool
SIPVoIPLink::sendRegister()
{
  int expire_value = Manager::instance().getRegistrationExpireValue();
  _debug("SIP Registration Expire Value = %i\n" , expire_value);

  if (_eXosipRegID != EXOSIP_ERROR_STD) {
    return false;
  }

  std::string hostname = getHostName();
  if (hostname.empty()) {
    return false;
  }

  if (_authname.empty()) {
    return false;
  }

  std::string proxy = "sip:" + _proxy;
  hostname = "sip:" + hostname;
  std::string from = SIPFromHeader(_authname, getHostName());
  
  osip_message_t *reg = NULL;
  eXosip_lock();
  if (!_proxy.empty()) {
    _debug("* SIP Info: Register from: %s to %s\n", from.data(), proxy.data());
    _eXosipRegID = eXosip_register_build_initial_register(from.data(), 
                  proxy.data(), NULL, expire_value, &reg);
  } else {
    _debug("* SIP Info: Register from: %s to %s\n", from.data(), hostname.data());
    _eXosipRegID = eXosip_register_build_initial_register(from.data(), 
                  hostname.data(), NULL, expire_value, &reg);
  }
  eXosip_unlock();
  if (_eXosipRegID < EXOSIP_ERROR_NO ) {
    return false;
  }

  if (!sendSIPAuthentification()) {
    _debug("* SIP Info: register without authentication\n");
    return false;
  }

  osip_message_set_header (reg, "Event", "Registration");
  osip_message_set_header (reg, "Allow-Events", "presence");

  eXosip_lock();
  int eXosipErr = eXosip_register_send_register(_eXosipRegID, reg);
  if (eXosipErr == EXOSIP_ERROR_BUILDING) {
    _debug("! SIP Failure: Cannot build registration, check the setup\n"); 
    eXosip_unlock();
    return false;
  }
  if (eXosipErr == EXOSIP_ERROR_STD) {
    _debug("! SIP Failure: Registration sending failed\n");
    eXosip_unlock();
    return false;
  }

  setRegistrationState(Trying);
  eXosip_unlock();

  return true;
}

std::string
SIPVoIPLink::SIPFromHeader(const std::string& userpart, const std::string& hostpart) 
{
  return ("\"" + getFullName() + "\"" + " <sip:" + userpart + "@" + hostpart + ">");
}

bool
SIPVoIPLink::sendSIPAuthentification() 
{
  std::string login = _authname;
  if (login.empty()) {
    /** @todo Ajouter ici un call à setRegistrationState(Error, "Fill balh") ? */
    return false;
  }
  if (_password.empty()) {
    /** @todo Même chose ici  ? */
    return false;
  }
  eXosip_lock();
  int returnValue = eXosip_add_authentication_info(login.data(), login.data(), _password.data(), NULL, NULL);
  eXosip_unlock();

  return (returnValue != EXOSIP_ERROR_STD ? true : false);
}

bool
SIPVoIPLink::sendUnregister()
{
  _debug("SEND UNREGISTER for account %s\n" , getAccountID().c_str());
  if ( _eXosipRegID == EXOSIP_ERROR_STD) return false;
  int eXosipErr = EXOSIP_ERROR_NO;
  osip_message_t *reg = NULL;

  eXosip_lock();
  eXosipErr = eXosip_register_build_register (_eXosipRegID, 0, &reg);
  eXosip_unlock();

  if (eXosipErr != EXOSIP_ERROR_NO) {
    _debug("! SIP Failure: Unable to build registration for sendUnregister");
    return false;
  }

  eXosip_lock();
  _debug("< Sending REGISTER (expire=0)\n");
  eXosipErr = eXosip_register_send_register (_eXosipRegID, reg);
  if (eXosipErr == EXOSIP_ERROR_BUILDING) {
    _debug("! SIP Failure: Cannot build registration (unregister), check the setup\n"); 
    eXosip_unlock();
    return false;
  }
  if (eXosipErr == EXOSIP_ERROR_STD) {
    _debug("! SIP Failure: Unable to send registration (unregister)\n");
  }
  _eXosipRegID = EXOSIP_ERROR_STD;
  eXosip_unlock();


  return true;
}

Call* 
SIPVoIPLink::newOutgoingCall(const CallID& id, const std::string& toUrl)
{
  SIPCall* call = new SIPCall(id, Call::Outgoing);
  if (call) {
    call->setPeerNumber(toUrl);
    // we have to add the codec before using it in SIPOutgoingInvite...
    call->setCodecMap(Manager::instance().getCodecDescriptorMap());
    if ( SIPOutgoingInvite(call) ) {
      call->setConnectionState(Call::Progressing);
      call->setState(Call::Active);
      addCall(call);
    } else {
      delete call; call = 0;
    }
  }
  return call;
}

bool
SIPVoIPLink::answer(const CallID& id)
{
  _debug("- SIP Action: start answering\n");

  SIPCall* call = getSIPCall(id);
  if (call==0) {
    _debug("! SIP Failure: SIPCall doesn't exists\n");
    return false;
  }

  // Send 200 OK
  osip_message_t *answerMessage = NULL;
  eXosip_lock();
  int i = eXosip_call_build_answer(call->getTid(), SIP_OK, &answerMessage);
  if (i != 0) {
    _debug("< SIP Building Error: send 400 Bad Request\n");
    eXosip_call_send_answer (call->getTid(), SIP_BAD_REQUEST, NULL);
  } else {
    // use exosip, bug locked
    i = 0;
    sdp_message_t *remote_sdp = eXosip_get_remote_sdp(call->getDid());
    if (remote_sdp!=NULL) {
      i = call->sdp_complete_message(remote_sdp, answerMessage);
      if (i!=0) {
        osip_message_free(answerMessage);
      }
      sdp_message_free(remote_sdp);
    }
    if (i != 0) {
      _debug("< SIP Error: send 415 Unsupported Media Type\n");
      eXosip_call_send_answer (call->getTid(), SIP_UNSUPPORTED_MEDIA_TYPE, NULL);
    } else {
      _debug("< SIP send 200 OK\n");
      eXosip_call_send_answer (call->getTid(), SIP_OK, answerMessage);
    }
  }
  eXosip_unlock();

  if(i==0) {
    // Incoming call is answered, start the sound thread.
    _debug("* SIP Info: Starting AudioRTP when answering\n");
    if (_audiortp.createNewSession(call) >= 0) {
      call->setAudioStart(true);
      call->setConnectionState(Call::Connected);
      call->setState(Call::Active);
      return true;
    } else {
      _debug("! SIP Failure: Unable to start sound when answering %s/%d\n", __FILE__, __LINE__);
    }
  }
  removeCall(call->getCallId());
  return false;
}

bool
SIPVoIPLink::hangup(const CallID& id)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("! SIP Error: Call doesn't exist\n"); return false; }  

  _debug("- SIP Action: Hang up call %s [cd: %3d %3d]\n", id.data(), call->getCid(), call->getDid()); 
  // Release SIP stack.
  eXosip_lock();
  eXosip_call_terminate(call->getCid(), call->getDid());
  eXosip_unlock();

  // Release RTP thread
  if (Manager::instance().isCurrentCall(id)) {
    _debug("* SIP Info: Stopping AudioRTP for hangup\n");
    _audiortp.closeRtpSession();
  }
  removeCall(id);
  return true;
}

bool
SIPVoIPLink::cancel(const CallID& id)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("! SIP Error: Call doesn't exist\n"); return false; }  

  _debug("- SIP Action: Cancel call %s [cid: %3d]\n", id.data(), call->getCid()); 
  // Release SIP stack.
  eXosip_lock();
  eXosip_call_terminate(call->getCid(), -1);
  eXosip_unlock();

  removeCall(id);
  return true;
}

bool
SIPVoIPLink::onhold(const CallID& id)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("! SIP Error: call doesn't exist\n"); return false; }  

  // Stop sound
  call->setAudioStart(false);
  call->setState(Call::Hold);
  _debug("* SIP Info: Stopping AudioRTP for onhold action\n");
  _audiortp.closeRtpSession();


  int did = call->getDid();

  eXosip_lock ();
  sdp_message_t *local_sdp = eXosip_get_local_sdp(did);
  eXosip_unlock ();

  if (local_sdp == NULL) {
    _debug("! SIP Failure: unable to find local_sdp\n");
    return false;
  }

  // Build INVITE_METHOD for put call on-hold
  osip_message_t *invite = NULL;
  eXosip_lock ();
  int exosipErr = eXosip_call_build_request (did, INVITE_METHOD, &invite);
  eXosip_unlock ();

  if (exosipErr != 0) {
    sdp_message_free(local_sdp);
    _debug("! SIP Failure: unable to build invite method to hold call\n");
    return false;
  }

  /* add sdp body */
  {
    char *tmp = NULL;

    int i = sdp_hold_call(local_sdp);
    if (i != 0) {
      sdp_message_free (local_sdp);
      osip_message_free (invite);
      _debug("! SIP Failure: Unable to hold call in SDP\n");
      return false;
    }
    
    i = sdp_message_to_str(local_sdp, &tmp);
    sdp_message_free(local_sdp);
    if (i != 0) {
      osip_message_free (invite);
      osip_free (tmp);
      _debug("! SIP Failure: Unable to translate sdp message to string\n");
      return false;
    }
    osip_message_set_body (invite, tmp, strlen (tmp));
    osip_free (tmp);
    osip_message_set_content_type (invite, "application/sdp");
  }
  
  // send request
  _debug("< SIP: Send on hold request\n");
  eXosip_lock ();
  exosipErr = eXosip_call_send_request (did, invite);
  eXosip_unlock ();
  
  return true;
}

bool 
SIPVoIPLink::offhold(const CallID& id)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("! SIP Error: Call doesn't exist\n"); return false; }  

  int did = call->getDid();

  eXosip_lock ();
  sdp_message_t *local_sdp = eXosip_get_local_sdp(did);
  eXosip_unlock ();

  if (local_sdp == NULL) {
    _debug("! SIP Failure: unable to find local_sdp\n");
    return false;
  }

  // Build INVITE_METHOD for put call off-hold
  osip_message_t *invite;
  eXosip_lock ();
  int exosipErr = eXosip_call_build_request (did, INVITE_METHOD, &invite);
  eXosip_unlock ();

  if (exosipErr != 0) {
    sdp_message_free(local_sdp);
    return EXOSIP_ERROR_STD;
  }

  /* add sdp body */
  {
    char *tmp = NULL;
    
    int i = sdp_off_hold_call (local_sdp);
    if (i != 0) {
      sdp_message_free (local_sdp);
      osip_message_free (invite);
      return false;
    }
    
    i = sdp_message_to_str (local_sdp, &tmp);
    sdp_message_free (local_sdp);
    if (i != 0) {
      osip_message_free (invite);
      osip_free (tmp);
      return false;
    }
    osip_message_set_body (invite, tmp, strlen (tmp));
    osip_free (tmp);
    osip_message_set_content_type (invite, "application/sdp");
  }

  // Send request
  _debug("< Send off hold request\n");
  eXosip_lock ();
  exosipErr = eXosip_call_send_request (did, invite);
  eXosip_unlock ();

  // Enable audio
  _debug("* SIP Info: Starting AudioRTP when offhold\n");
  call->setState(Call::Active);
  // it's sure that this is the current call id...
  if (_audiortp.createNewSession(call) < 0) {
    _debug("! SIP Failure: Unable to start sound (%s:%d)\n", __FILE__, __LINE__);
    return false;
  }
  return true;
}

bool 
SIPVoIPLink::transfer(const CallID& id, const std::string& to)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("! SIP Failure: Call doesn't exist\n"); return false; }  

  std::string tmp_to = SIPToHeader(to);
  if (tmp_to.find("@") == std::string::npos) {
    tmp_to = tmp_to + "@" + getHostName();
  }

  osip_message_t *refer;
  eXosip_lock();
  // Build transfer request
  int exosipErr = eXosip_call_build_refer(call->getDid(), (char*)tmp_to.data(), &refer);
  if (exosipErr == 0) {
    // Send transfer request
    _debug("< SIP send transfer request to %s\n", tmp_to.data());
    exosipErr = eXosip_call_send_request(call->getDid(), refer);
  }
  eXosip_unlock();

  _audiortp.closeRtpSession();
  // shall we delete the call?
  //removeCall(id);
  return true;
}

bool
SIPVoIPLink::refuse (const CallID& id)
{
  SIPCall* call = getSIPCall(id);

  if (call==0) { _debug("Call doesn't exist\n"); return false; }  

  // can't refuse outgoing call or connected
  if (!call->isIncoming() || call->getConnectionState() == Call::Connected) { 
    _debug("It's not an incoming call, or it's already answered\n");
    return false; 
  }


  osip_message_t *answerMessage = NULL;
  eXosip_lock();
  // not BUSY.. where decline the invitation!
  int exosipErr = eXosip_call_build_answer(call->getTid(), SIP_DECLINE, &answerMessage);
  if (exosipErr == 0) {
    exosipErr = eXosip_call_send_answer(call->getTid(), SIP_DECLINE, answerMessage);
  }
  eXosip_unlock();
  return true;
}

bool 
SIPVoIPLink::carryingDTMFdigits(const CallID& id, char code)
{
  SIPCall* call = getSIPCall(id);
  if (call==0) { _debug("Call doesn't exist\n"); return false; }  

  int duration = Manager::instance().getConfigInt(SIGNALISATION, PULSE_LENGTH);
  osip_message_t *info;
  const int body_len = 1000;
  int i;

  char *dtmf_body = new char[body_len];

  eXosip_lock();
  // Build info request
  i = eXosip_call_build_info(call->getDid(), &info);
  if (i == 0) {
    snprintf(dtmf_body, body_len - 1, "Signal=%c\r\nDuration=%d\r\n", code, duration);
    osip_message_set_content_type (info, "application/dtmf-relay");
    osip_message_set_body (info, dtmf_body, strlen (dtmf_body));
    // Send info request
    i = eXosip_call_send_request(call->getDid(), info);
  }
  eXosip_unlock();
  
  delete[] dtmf_body; dtmf_body = NULL;
  return true;
}

bool
SIPVoIPLink::sendMessage(const std::string& to, const std::string& body)
{
  bool returnValue = false;

  // fast return
  if (body.empty()) {return returnValue; }

  osip_message_t* message = 0;
  const char* method = "MESSAGE";

  std::string sipFrom = getSipFrom();
  std::string sipTo   = getSipTo(to);
  std::string sipRoute = getSipRoute();

  if (!SIPCheckUrl(sipFrom)) {
    return returnValue;
  }
  if (!SIPCheckUrl(sipTo)) {
    return returnValue;
  }

  int eXosipError = EXOSIP_ERROR_STD;
  eXosip_lock();
  if ( sipRoute.empty() ) {
    eXosipError = eXosip_message_build_request(&message, method, sipTo.c_str(), sipFrom.c_str(), NULL);
  } else {
    eXosipError = eXosip_message_build_request(&message, method, sipTo.c_str(), sipFrom.c_str(), sipRoute.c_str());
  }

  if (eXosipError == EXOSIP_ERROR_NO) {
    // add body
    // add message
    // src: http://www.atosc.org/pipermail/public/osip/2005-October/006007.html
    osip_message_set_expires(message, "120");
    osip_message_set_body(message, body.c_str(), body.length());
    osip_message_set_content_type(message, "text/plain");

    eXosipError = eXosip_message_send_request(message);
    if (eXosipError == EXOSIP_ERROR_NO) {
      // correctly send the message
      returnValue = true;
    }
  }
  eXosip_unlock();
  return returnValue;
}

// NOW
bool
SIPVoIPLink::isContactPresenceSupported()
{
	return true;
}

/*
void
SIPVoIPLink::subscribePresenceForContact(Contact* contact)
{
	osip_message_t* subscription;
	
	int i;
	
	std::string to   = contact->getUrl().data();
	std::ostringstream from;
	
	// Build URL of sender
	from << "sip:" << _authname.data() << "@" << getHostName().data();

	// Subscribe for changes on server but also polls at every 5000 interval
	i = eXosip_subscribe_build_initial_request(&subscription,
			to.data(),
			from.str().c_str(),
			NULL,
			"presence", 5000);
	if(i!=0) return;
	
	// We want to receive presence in the PIDF XML format in SIP messages
	osip_message_set_accept(subscription, "application/pidf+xml");
	
	// Send subscription
	eXosip_lock();
	i = eXosip_subscribe_send_initial_request(subscription);
	if(i!=0) _debug("Sending of subscription tp %s failed\n", to.data());
	eXosip_unlock();
}
*/

void
SIPVoIPLink::publishPresenceStatus(std::string status)
{
	_debug("Publishing presence status\n");
	char buf[4096];
	int i;
	osip_message_t* publication;
	
	std::ostringstream url;
	std::string basic;
	std::string note;
	
	// Build URL of sender
	url << "sip:" << _authname.data() << "@" << getHostName().data();
	
	// TODO
	// Call function to convert status in basic and note
	// tags that are integrated in the publication
	basic = "open";
	note = "ready";
	
	snprintf(buf, 4096,
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<presence xmlns=\"urn:ietf:params:xml:ns:pidf\"\n\
          xmlns:es=\"urn:ietf:params:xml:ns:pidf:status:rpid-status\"\n\
          entity=\"%s\">\n\
	<tuple id=\"sg89ae\">\n\
		<status>\n\
			<basic>%s</basic>\n\
			<es:activities>\n\
				<es:activity>in-transit</es:activity>\n\
			</es:activities>\n\
		</status>\n\
		<contact priority=\"0.8\">%s</contact>\n\
		<note>%s</note>\n\
	</tuple>\n\
</presence>"
			, url.str().c_str(), basic.data(), url.str().c_str(), note.data());
	
	// Build publish request in PIDF
	i = eXosip_build_publish(&publication, url.str().c_str(), url.str().c_str(), NULL, "presence", "1800", "application/pidf+xml", buf);
	
	eXosip_lock();
	i = eXosip_publish(publication, url.str().c_str());
	eXosip_unlock();
}

bool
SIPVoIPLink::SIPOutgoingInvite(SIPCall* call) 
{
  // If no SIP proxy setting for direct call with only IP address
  if (!SIPStartCall(call, "")) {
    _debug("! SIP Failure: call not started\n");
    return false;
  }
  return true;
}

bool
SIPVoIPLink::SIPStartCall(SIPCall* call, const std::string& subject) 
{
  if (!call) return false;

  std::string to    = getSipTo(call->getPeerNumber());
  std::string from  = getSipFrom();
  std::string route = getSipRoute();
  _debug("            From: %s\n", from.data());
  _debug("            Route: %s\n", route.data());

  if (!SIPCheckUrl(from)) {
    _debug("! SIP Error: Source address is invalid %s\n", from.data());
    return false;
  }
  if (!SIPCheckUrl(to)) {
    return false;
  }

  osip_message_t *invite;
  eXosip_lock();
  int eXosipError = eXosip_call_build_initial_invite (&invite, (char*)to.data(),
                                        (char*)from.data(),
                                        (char*)route.data(),
                                        (char*)subject.data());
  
  if (eXosipError != 0) {
    eXosip_unlock();
    return false; // error when building the invite
  }

  setCallAudioLocal(call);

  std::ostringstream media_audio;
  std::ostringstream rtpmap_attr;
  AudioCodecType payload;
  int nbChannel;
  int iter;

  // Set rtpmap according to the supported codec order
  //CodecMap map = call->getCodecMap().getCodecMap();
  CodecOrder map = call->getCodecMap().getActiveCodecs();
 
  for(iter=0 ; iter < map.size() ; iter++){
      if(map[iter] != -1){
	payload = map[iter];
        // add each payload in the list of payload
        media_audio << payload << " ";

        rtpmap_attr << "a=rtpmap:" << payload << " " << 
        call->getCodecMap().getCodecName(payload) << "/" << call->getCodecMap().getSampleRate(payload);

    	//TODO add channel infos
        nbChannel = call->getCodecMap().getChannel(payload);
        if (nbChannel!=1) {
          rtpmap_attr << "/" << nbChannel;
        }
        rtpmap_attr << "\r\n";
      }
    // go to next codec
    //*iter++;
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
              _localExternAddress.c_str(), _localExternAddress.c_str(), call->getLocalExternAudioPort(), media_audio.str().c_str(), rtpmap_attr.str().c_str());
    // media_audio should be one, two or three numbers?
    osip_message_set_body (invite, tmp, strlen (tmp));
    osip_message_set_content_type (invite, "application/sdp");
    _debug("SDP send: %s", tmp);
  }
  
  _debug("> INVITE To <%s>\n", to.data());
  int cid = eXosip_call_send_initial_invite(invite);

  // Keep the cid in case of cancelling
  call->setCid(cid);

  if (cid <= 0) {
    eXosip_unlock();
    return false ;
  } else {
    _debug("* SIP Info: Outgoing callID is %s, cid=%d\n", call->getCallId().data(), cid);
    eXosip_call_set_reference (cid, NULL);
  }
  eXosip_unlock();

  return true;
}

std::string
SIPVoIPLink::getSipFrom() {

  // Form the From header field basis on configuration panel
  std::string host = getHostName();
  if ( host.empty() ) {
    host = _localIPAddress;
  }
  return SIPFromHeader(_authname, host);
}

std::string
SIPVoIPLink::getSipTo(const std::string& to_url) {
  // Form the From header field basis on configuration panel
  bool isRegistered = (_eXosipRegID == EXOSIP_ERROR_STD) ? false : true;

  // add a @host if we are registered and there is no one inside the url
  if (to_url.find("@") == std::string::npos && isRegistered) {
    std::string host = getHostName();
    if(!host.empty()) {
      return SIPToHeader(to_url + "@" + host);
    }
  }
  return SIPToHeader(to_url);
}

std::string
SIPVoIPLink::getSipRoute() {
  std::string proxy = _proxy;
  if ( !proxy.empty() ) {
    proxy = "<sip:" + proxy + ";lr>";
  }
  return proxy; // return empty
}

std::string
SIPVoIPLink::SIPToHeader(const std::string& to) 
{
  if (to.find("sip:") == std::string::npos) {
    return ("sip:" + to );
  } else {
    return to;
  }
}

bool
SIPVoIPLink::SIPCheckUrl(const std::string& url)
{
  int i;

  osip_from_t *to;
  i = osip_from_init(&to);
  if (i != 0) {
    _debug("! SIP Warning: Cannot initialize osip parser\n");
    return false;
  }
  i = osip_from_parse(to, url.data());
  if (i != 0) {
    _debug("! SIP Warning: Cannot parse url %s\n", url.data());
    return false;
  }

  // Free memory
  osip_from_free (to);
  return true;
}

bool
SIPVoIPLink::setCallAudioLocal(SIPCall* call) 
{
  // Setting Audio
  unsigned int callLocalAudioPort = RANDOM_LOCAL_PORT;
  unsigned int callLocalExternAudioPort = callLocalAudioPort;
  if (_useStun) {
    // If use Stun server
    if (Manager::instance().behindNat(_stunServer, callLocalAudioPort)) {
      callLocalExternAudioPort = Manager::instance().getFirewallPort();
    }
  }
  _debug("            Setting local audio port to: %d\n", callLocalAudioPort);
  _debug("            Setting local audio port (external) to: %d\n", callLocalExternAudioPort);
  
  // Set local audio port for SIPCall(id)
  call->setLocalIp(_localIPAddress);
  call->setLocalAudioPort(callLocalAudioPort);
  call->setLocalExternAudioPort(callLocalExternAudioPort);

  return true;
}

void
SIPVoIPLink::SIPCallInvite(eXosip_event_t *event)
{
  _debug("> INVITE (receive)\n");
  CallID id = Manager::instance().getNewCallID();

  SIPCall* call = new SIPCall(id, Call::Incoming);
  if (!call) {
    _debug("! SIP Failure: unable to create an incoming call");
    return;
  }

  setCallAudioLocal(call);
  call->setCodecMap(Manager::instance().getCodecDescriptorMap());
  call->setConnectionState(Call::Progressing);
  if (call->SIPCallInvite(event)) {
    if (Manager::instance().incomingCall(call, getAccountID())) {
      addCall(call);
    } else {
      delete call; call = 0;
    }
  } else {
    delete call; call = 0;
  }
  // Send 180 RINGING
  _debug("< Send 180 Ringing\n");
  eXosip_lock ();
  eXosip_call_send_answer(event->tid, 180, NULL);
  eXosip_unlock ();
  call->setConnectionState(Call::Ringing);
}

void
SIPVoIPLink::SIPCallReinvite(eXosip_event_t *event)
{
  _debug("> REINVITE (receive)\n");
  SIPCall* call = findSIPCallWithCidDid(event->cid, event->did);
  if (call == 0) {
    _debug("! SIP Failure: unknown call\n");
    _debug("< Send 488 Not Acceptable Here");
    eXosip_lock();
    eXosip_call_send_answer(event->tid, 488, NULL);
    eXosip_unlock();
    return;
  }
  if ( call->getCallId() == Manager::instance().getCurrentCallId()) {
    // STOP tone
    Manager::instance().stopTone(true);
    // STOP old rtp session
    _debug("* SIP Info: Stopping AudioRTP when reinvite\n");
    _audiortp.closeRtpSession();
    call->setAudioStart(false);
  }
  call->SIPCallReinvite(event);
}

void
SIPVoIPLink::SIPCallRinging(eXosip_event_t *event)
{
  
  SIPCall* call = findSIPCallWithCid(event->cid);
  if (!call) {
    _debug("! SIP Failure: unknown call\n");
    return;
  }
  // we could set the cid/did/tid and get the FROM here...
  // but we found the call with the cid/did already, why setting it again?
  // call->ringingCall(event);
  call->setDid(event->did);
  call->setConnectionState(Call::Ringing);
  Manager::instance().peerRingingCall(call->getCallId());
}

void
SIPVoIPLink::SIPCallAnswered(eXosip_event_t *event)
{
  SIPCall* call = findSIPCallWithCid(event->cid);
  if (!call) {
    _debug("! SIP Failure: unknown call\n");
    return;
  }
  call->setDid(event->did);

  if (call->getConnectionState() != Call::Connected) {
    call->SIPCallAnswered(event);
    call->SIPCallAnsweredWithoutHold(event);

    call->setConnectionState(Call::Connected);
    call->setState(Call::Active);

    Manager::instance().peerAnsweredCall(call->getCallId());
    if (Manager::instance().isCurrentCall(call->getCallId())) {
      _debug("* SIP Info: Starting AudioRTP when answering\n");
      if ( _audiortp.createNewSession(call) < 0) {
        _debug("RTP Failure: unable to create new session\n");
      } else {
        call->setAudioStart(true);
      }
    }
  } else {
     _debug("* SIP Info: Answering call (on/off hold to send ACK)\n");
     call->SIPCallAnswered(event);
  }
}

void
SIPVoIPLink::SIPCallRequestFailure(eXosip_event_t *event)
{
  if (!event->response) { return; }
  // 404 error
  _debug("  Request Failure, receive code %d\n", event->response->status_code);
  // Handle 4XX errors
  switch (event->response->status_code) {
  case SIP_PROXY_AUTHENTICATION_REQUIRED: 
    _debug("- SIP Action: Server ask required authentification: logging...\n");
    sendSIPAuthentification();
    eXosip_lock();
    eXosip_automatic_action();
    eXosip_unlock();
    break;
  case SIP_UNAUTHORIZED:
    _debug("- SIP Action: Request is unauthorized. SIP Server ask authentification: logging...\n");
    sendSIPAuthentification();
    break;

  case SIP_BUSY_HERE:  // 486
    {
      SIPCall* call = findSIPCallWithCid(event->cid);
      if (call!=0) {
        CallID& id = call->getCallId();
        call->setConnectionState(Call::Connected);
        call->setState(Call::Busy);
        Manager::instance().callBusy(id);
        removeCall(id);
      }
    }
    break;
  case SIP_REQUEST_TERMINATED: // 487
    break;

  default:
  /*case SIP_BAD_REQUEST:
  case SIP_FORBIDDEN:
  case SIP_NOT_FOUND:
  case SIP_METHOD_NOT_ALLOWED:
  case SIP_406_NOT_ACCEPTABLE:
  case SIP_REQ_TIME_OUT:
  case SIP_TEMPORARILY_UNAVAILABLE:
  case SIP_ADDRESS_INCOMPLETE:
  case SIP_NOT_ACCEPTABLE_HERE: // 488 */
    // Display error on the screen phone
    {
      SIPCall* call = findSIPCallWithCid(event->cid);
      if (call!=0) {
        CallID& id = call->getCallId();
        call->setConnectionState(Call::Connected);
        call->setState(Call::Error);
        Manager::instance().callFailure(id);
        removeCall(id);
      }
    }
  }
}

void
SIPVoIPLink::SIPCallServerFailure(eXosip_event_t *event) 
{
  if (!event->response) { return; }
  switch(event->response->status_code) {
  case SIP_SERVICE_UNAVAILABLE: // 500
  case SIP_BUSY_EVRYWHERE:     // 600
  case SIP_DECLINE:             // 603
    SIPCall* call = findSIPCallWithCid(event->cid);
    if (call != 0) {
      CallID id = call->getCallId();
      Manager::instance().callFailure(id);
      removeCall(id);
    }
  break;
  }
}

void
SIPVoIPLink::SIPRegistrationFailure( eXosip_event_t* event )
{
  if(!event->response){
    setRegistrationState(ErrorHost);
    return ;
  }

  switch(  event->response->status_code ) {
    case SIP_FORBIDDEN:
      _debug("SIP forbidden\n");
      setRegistrationState(ErrorAuth);
      break;
    case SIP_UNAUTHORIZED:
      _debug("SIP unauthorized\n");
      setRegistrationState(Error);
      break;
    default:
      setRegistrationState(ErrorAuth);
      //_debug("Unknown error: %s\n" , event->response->status_code);
  }
}

void
SIPVoIPLink::SIPCallAck(eXosip_event_t *event) 
{
  SIPCall* call = findSIPCallWithCidDid(event->cid, event->did);
  if (!call) { return; }
  if (!call->isAudioStarted()) {
    if (Manager::instance().isCurrentCall(call->getCallId())) {
      _debug("* SIP Info: Starting AudioRTP when ack\n");
      if ( _audiortp.createNewSession(call) ) {
        call->setAudioStart(true);
      }
    }
  }
}

void
SIPVoIPLink::SIPCallMessageNew(eXosip_event_t *event) 
{
  if (0 == event->request) return;

  _debug("  > SIP Event: Receive a call message\n");

  if (MSG_IS_INFO(event->request)) {
    _debug("* SIP Info: It's a Request Info\n");
    osip_content_type_t* c_t = event->request->content_type;
    if (c_t != 0 && c_t->type != 0 && c_t->subtype != 0 ) {
      _debug("* SIP Info: Content Type of the message: %s/%s\n", c_t->type, c_t->subtype);
      // application/dtmf-relay
      if (strcmp(c_t->type, "application") == 0 && strcmp(c_t->subtype, "dtmf-relay") == 0) {
         handleDtmfRelay(event);
      }
    }
  }

  osip_message_t *answerOKNewMessage;
  eXosip_lock();
  if ( 0 == eXosip_call_build_answer(event->tid, SIP_OK, &answerOKNewMessage)) {
    _debug("< SIP Sending 200 OK\n");
    eXosip_call_send_answer(event->tid, SIP_OK, answerOKNewMessage);
  } else {
    _debug("! SIP Failure: Could not sent an OK message\n");
  }
  eXosip_unlock();

}

void
SIPVoIPLink::SIPCallClosed(eXosip_event_t *event) 
{
  // it was without did before
  SIPCall* call = findSIPCallWithCid(event->cid);
  if (!call) { return; }

  CallID id = call->getCallId();
  call->setDid(event->did);
  if (Manager::instance().isCurrentCall(id)) {
    call->setAudioStart(false);
    _debug("* SIP Info: Stopping AudioRTP when closing\n");
    _audiortp.closeRtpSession();
  }
  Manager::instance().peerHungupCall(id);
  removeCall(id);
}

void
SIPVoIPLink::SIPCallReleased(eXosip_event_t *event)
{
  // do cleanup if exists
  // only cid because did is always 0 in these case..
  SIPCall* call = findSIPCallWithCid(event->cid);
  if (!call) { return; }

  // if we are here.. something when wrong before...
  _debug("SIP call release\n");
  CallID id = call->getCallId();
  Manager::instance().callFailure(id);
  removeCall(id);
}

void
SIPVoIPLink::SIPMessageNew(eXosip_event_t *event)
{
  if (MSG_IS_OPTIONS(event->request)) {
    // old handling was
    // - send 200 OK if call id is not found
    // - send nothing if call id is found
    eXosip_lock();
    eXosip_options_send_answer (event->tid, SIP_OK, NULL);
    eXosip_unlock();
  }
  // Voice message 
  else if (MSG_IS_NOTIFY(event->request)){
    int ii;
    osip_body_t *body = NULL;
    // Get the message body
    ii = osip_message_get_body(event->request, 0, &body);
    if (ii != 0) {
      _debug("! SIP Error: Cannot get body in a new EXOSIP_MESSAGE_NEW event\n");
      return;
    }

    // Analyse message body
    if (!body || !body->body) {
       return;
    }
    std::string str(body->body);
    std::string::size_type pos;
    std::string::size_type pos_slash;
    pos = str.find(VOICE_MSG);

    if (pos == std::string::npos) {
      // If the string is not found
      return;
    } 

    pos_slash = str.find ("/");
    std::string nb_msg = str.substr(pos + LENGTH_VOICE_MSG, 
    pos_slash - (pos + LENGTH_VOICE_MSG));

    // Set the number of voice-message
    int msgVoicemail = atoi(nb_msg.data());
    _debug("  > NOTIFY ->  %i voice message for account %s\n" , msgVoicemail , getAccountID().c_str());

    if (msgVoicemail != 0) {
      // If there is at least one voice-message, start notification
      Manager::instance().startVoiceMessageNotification(getAccountID(), nb_msg);
    }
  // http://www.jdrosen.net/papers/draft-ietf-simple-im-session-00.txt
  } else if (MSG_IS_MESSAGE(event->request)) {
    _debug("> MESSAGE received\n");
    // osip_content_type_t* osip_message::content_type
    osip_content_type_t* c_t = event->request->content_type;
    if (c_t != 0 &&  c_t->type != 0 && c_t->subtype != 0 ) {
      _debug("* SIP Info: Content Type of the message: %s/%s\n", c_t->type, c_t->subtype);

      osip_body_t *body = NULL;
      // Get the message body
      if (0 == osip_message_get_body(event->request, 0, &body)) {
        _debug("* SIP Info: Body length: %d\n", body->length);
        if (body->body!=0 && 
            strcmp(c_t->type,"text") == 0 && 
            strcmp(c_t->subtype,"plain") == 0
          ) {
          _debug("* SIP Info: Text body: %s\n", body->body);
          Manager::instance().incomingMessage(getAccountID(), body->body);
        }
      }
    }
    osip_message_t *answerOK;
    eXosip_lock();
    if ( 0 == eXosip_message_build_answer(event->tid, SIP_OK, &answerOK)) {
        _debug("< Sending 200 OK\n");
        eXosip_message_send_answer(event->tid, SIP_OK, answerOK);
    }
    eXosip_unlock();
  }

}

SIPCall* 
SIPVoIPLink::findSIPCallWithCid(int cid) 
{
  if (cid < 1) {
    _debug("! SIP Error: Not enough information for this event\n");
    return NULL;
  }
  ost::MutexLock m(_callMapMutex);
  SIPCall* call = 0;
  CallMap::iterator iter = _callMap.begin();
  while(iter != _callMap.end()) {
    call = dynamic_cast<SIPCall*>(iter->second);
    if (call && call->getCid() == cid) {
      return call;
    }
    iter++;
  }
  return NULL;
}

SIPCall* 
SIPVoIPLink::findSIPCallWithCidDid(int cid, int did) 
{
  if (cid < 1 && did < -1) {
    _debug("! SIP Error: Not enough information for this event\n");
    return NULL;
  }
  ost::MutexLock m(_callMapMutex);
  SIPCall* call = 0;
  CallMap::iterator iter = _callMap.begin();
  while(iter != _callMap.end()) {
    call = dynamic_cast<SIPCall*>(iter->second);
    if (call && call->getCid() == cid && call->getDid() == did) {
      return call;
    }
    iter++;
  }
  return NULL;
}

SIPCall*
SIPVoIPLink::getSIPCall(const CallID& id) 
{
  Call* call = getCall(id);
  if (call) {
    return dynamic_cast<SIPCall*>(call);
  }
  return NULL;
}

bool
SIPVoIPLink::handleDtmfRelay(eXosip_event_t* event) {

  SIPCall* call = findSIPCallWithCidDid(event->cid, event->did);
  if (call==0) { return false; }


  bool returnValue = false;
  osip_body_t *body = NULL;
  // Get the message body
  if (0 == osip_message_get_body(event->request, 0, &body) && body->body != 0 )   {
    _debug("* SIP Info: Text body: %s\n", body->body);
    std::string dtmfBody(body->body);
    std::string::size_type posStart = 0;
    std::string::size_type posEnd = 0;
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
      _debug("* SIP Info: Signal value: %s\n", signal.c_str());
      
      if (!signal.empty()) {
        if (Manager::instance().isCurrentCall(call->getCallId())) {
          Manager::instance().playDtmf(signal[0], true);
          returnValue = true;
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

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////
int
SIPVoIPLink::sdp_hold_call (sdp_message_t * sdp)
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
SIPVoIPLink::sdp_off_hold_call (sdp_message_t * sdp)
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
