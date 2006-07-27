/*
 *  Copyright (C) 2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#include "iaxvoiplink.h"
#include "global.h" // for _debug
#include "iaxcall.h"
#include "eventthread.h"

#include "manager.h"

#define IAX_SUCCESS  0
#define IAX_FAILURE -1

#define RANDOM_IAX_PORT   rand() % 64000 + 1024

// from IAXC : iaxclient.h

/* payload formats : WARNING: must match libiax values!!! */
/* Data formats for capabilities and frames alike */
#define IAX__FORMAT_G723_1       (1 << 0)        /* G.723.1 compression */
#define IAX__FORMAT_GSM          (1 << 1)        /* GSM compression */
#define IAX__FORMAT_ULAW         (1 << 2)        /* Raw mu-law data (G.711) */
#define IAX__FORMAT_ALAW         (1 << 3)        /* Raw A-law data (G.711) */
#define IAX__FORMAT_G726         (1 << 4)        /* ADPCM, 32kbps  */
#define IAX__FORMAT_ADPCM        (1 << 5)        /* ADPCM IMA */
#define IAX__FORMAT_SLINEAR      (1 << 6)        /* Raw 16-bit Signed Linear (8000 Hz) PCM */
#define IAX__FORMAT_LPC10        (1 << 7)        /* LPC10, 180 samples/frame */
#define IAX__FORMAT_G729A        (1 << 8)        /* G.729a Audio */
#define IAX__FORMAT_SPEEX        (1 << 9)        /* Speex Audio */
#define IAX__FORMAT_ILBC         (1 << 10)       /* iLBC Audio */

#define IAX__FORMAT_MAX_AUDIO    (1 << 15)  /* Maximum audio format */
#define IAX__FORMAT_JPEG         (1 << 16)       /* JPEG Images */
#define IAX__FORMAT_PNG          (1 << 17)       /* PNG Images */
#define IAX__FORMAT_H261         (1 << 18)       /* H.261 Video */
#define IAX__FORMAT_H263         (1 << 19)       /* H.263 Video */
#define IAX__FORMAT_H263_PLUS    (1 << 20)       /* H.263+ Video */
#define IAX__FORMAT_MPEG4        (1 << 21)       /* MPEG4 Video */
#define IAX__FORMAT_H264         (1 << 23)       /* H264 Video */
#define IAX__FORMAT_THEORA       (1 << 24)       /* Theora Video */

IAXVoIPLink::IAXVoIPLink(const AccountID& accountID)
 : VoIPLink(accountID)
{
  _evThread = new EventThread(this);
  _regSession = 0;

  // to get random number for RANDOM_PORT
  srand (time(NULL));
}


IAXVoIPLink::~IAXVoIPLink()
{
  delete _evThread; _evThread = 0;
  _regSession = 0; // shall not delete it
  terminate();
}

bool
IAXVoIPLink::init()
{
  bool returnValue = false;
  //_localAddress = "127.0.0.1";
  // port 0 is default
  //  iax_enable_debug(); have to enable debug when compiling iax...
  int port = IAX_DEFAULT_PORTNO;
  int last_port = 0;
  int nbTry = 3;

  while (port != IAX_FAILURE && nbTry) {
    last_port = port;
    port = iax_init(port);
    if ( port < 0 ) {
      _debug("IAX Warning: already initialize on port %d\n", last_port);
      port = RANDOM_IAX_PORT;
    } else if (port == IAX_FAILURE) {
      _debug("IAX Fail to start on port %d", last_port);
      port = RANDOM_IAX_PORT;
    } else {
      _debug("IAX Info: listening on port %d\n", last_port);
      _localPort = last_port;
      returnValue = true;

      _evThread->start();
      break;
    }
    nbTry--;
  }
  if (port == IAX_FAILURE || nbTry==0) {
    _debug("Fail to initialize iax\n");
  }
  return returnValue;
}

void
IAXVoIPLink::terminate()
{
//  iaxc_shutdown();  
//  hangup all call
//  iax_hangup(calls[callNo].session,"Dumped Call");
}

void
IAXVoIPLink::getEvent() 
{
  // mutex here
  _mutexIAX.enterMutex();

  iax_event* event = 0;
  IAXCall* call = 0;
  while ( (event = iax_get_event(0)) != 0 ) {
    _debug ("Receive IAX Event: %d\n", event->etype);
    call = iaxFindCallBySession(event->session);
    if (call!=0) {
      iaxHandleCallEvent(event, call);
    } else if (event->session != 0 && event->session == _regSession) {
      // in iaxclient, there is many session handling, here, only one
      iaxHandleRegReply(event);
    } else {
      switch(event->etype) {
        case IAX_EVENT_REGACK:
        case IAX_EVENT_REGREJ:
          _debug("Unknown IAX Registration Event\n");
        break;

        case IAX_EVENT_REGREQ:
          _debug("Registration by a peer, don't allow it\n");
        break;
        case IAX_EVENT_CONNECT: // new call
          // New incoming call!	
        break;

        case IAX_EVENT_TIMEOUT: // timeout for an unknown session

        break;

        default:
          _debug("Unknown event type: %d\n", event->etype);
      }
    }
    iax_event_free(event);
  }
  // unlock mutex here
  _mutexIAX.leaveMutex();
  //iaxRefreshRegistrations();

  // thread wait 5 millisecond
  _evThread->sleep(5);
}

bool
IAXVoIPLink::setRegister() 
{
  bool result = false;
  if (_regSession==0) {
    if (_host.empty()) {
      Manager::instance().displayConfigError("Fill host field for IAX Account");
      return false;
    }
    if (_user.empty()) {
      Manager::instance().displayConfigError("Fill user field for IAX Account");
      return false;
    }

    // lock
    _mutexIAX.enterMutex();
    _regSession = iax_session_new();

    if (!_regSession) {
      _debug("error when generating new session for register");
    } else {
      // refresh
      // last reg
      char host[_host.length()+1]; 
      strcpy(host, _host.c_str());
      char user[_user.length()+1];
      strcpy(user, _user.c_str());
      char pass[_pass.length()+1]; 
      strcpy(pass, _pass.c_str());
      //iax_register don't use const char*

      _debug("IAX Sending registration to %s with user %s\n", host, user);
      int val = iax_register(_regSession, host, user, pass, 300);
      _debug ("Return value: %d\n", val);
      result = true;
    }

    _mutexIAX.leaveMutex();
  }
  return result;
}

bool
IAXVoIPLink::setUnregister()
{
  if (_regSession==0) {
    // lock here
    _mutexIAX.enterMutex();
    iax_destroy(_regSession);
    _regSession = 0;
    // unlock here
    _mutexIAX.leaveMutex();
    return false;
  }
  return false;
}

Call* 
IAXVoIPLink::newOutgoingCall(const CallID& id, const std::string& toUrl)
{
  IAXCall* call = new IAXCall(id, Call::Outgoing);
  if (call) {
    call->setPeerNumber(toUrl);
    // we have to add the codec before using it in SIPOutgoingInvite...
    //call->setCodecMap(Manager::instance().getCodecDescriptorMap());
    if ( iaxOutgoingInvite(call) ) {
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
IAXVoIPLink::iaxOutgoingInvite(IAXCall* call) 
{
  struct iax_session *newsession;
  // lock here
  _mutexIAX.enterMutex();
  newsession = iax_session_new();
  if (!newsession) {
     _debug("IAX Error: Can't make new session for a new call\n");
     // unlock here
     _mutexIAX.leaveMutex();
     return false;
  }
  call->setSession(newsession);
  /* reset activity and ping "timers" */
  // iaxc_note_activity(callNo);
  char num[call->getPeerNumber().length()+1];
  strcpy(num, call->getPeerNumber().c_str());

  char* lang = NULL;
  int wait = 0;
  int audio_format_preferred =  IAX__FORMAT_SPEEX;
  int audio_format_capability = IAX__FORMAT_ULAW | IAX__FORMAT_ALAW | IAX__FORMAT_GSM | IAX__FORMAT_SPEEX;

  iax_call(newsession, num, num, num, lang, wait, audio_format_preferred, audio_format_capability);

  // unlock here
  _mutexIAX.leaveMutex();
  return true;
}


IAXCall* 
IAXVoIPLink::iaxFindCallBySession(struct iax_session* session) 
{
  // access to callMap shoud use that
  // the code below is like findSIPCallWithCid() 
  ost::MutexLock m(_callMapMutex);	
  IAXCall* call = 0;
  CallMap::iterator iter = _callMap.begin();
  while(iter != _callMap.end()) {
    call = dynamic_cast<IAXCall*>(iter->second);
    if (call && call->getSession() == session) {
      return call;
    }
    iter++;
  }
  return 0; // not found
}

void
IAXVoIPLink::iaxHandleCallEvent(iax_event* event, IAXCall* call) 
{
  // call should not be 0
  // note activity?
  //
  switch(event->etype) {
    case IAX_EVENT_HANGUP:
    break;

    case IAX_EVENT_REJECT:
    break;

    case IAX_EVENT_ACCEPT:
    break;

    case IAX_EVENT_ANSWER:
    break;
    
    case IAX_EVENT_BUSY:
    break;
    
    case IAX_EVENT_VOICE:
    break;
    
    case IAX_EVENT_TEXT:
    break;
    
    case IAX_EVENT_RINGA:
    break;
    
    case IAX_EVENT_PONG:
    break;
    
    case IAX_EVENT_URL:
    break;
    
//    case IAX_EVENT_CNG: ??
//    break;
    
    case IAX_EVENT_TIMEOUT:
    break;
    
    case IAX_EVENT_TRANSFER:
    break;
    
    default:
      _debug("Unknown event type: %d\n", event->etype);
    
  }
}



void
IAXVoIPLink::iaxHandleRegReply(iax_event* event) 
{
  //unregister
  if (event->etype == IAX_EVENT_REGREJ) {
    iax_destroy(_regSession);
    _regSession = 0;
    Manager::instance().registrationFailed(getAccountID());
  } else if (event->etype == IAX_EVENT_REGACK) {
    Manager::instance().registrationSucceed(getAccountID());
  }
}
