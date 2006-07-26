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

#define IAX_SUCCESS  0
#define IAX_FAILURE -1

IAXVoIPLink::IAXVoIPLink(const AccountID& accountID)
 : VoIPLink(accountID)
{
  _evThread = new EventThread(this);
  _regSession = 0;
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
  int port = iax_init(IAX_DEFAULT_PORTNO);
  if (port == IAX_FAILURE) {
    _debug("IAX Failure: Error when initializing\n");
  } else if ( port == 0 ) {
    _debug("IAX Warning: already initialize\n");
  } else {
    _debug("IAX Info: listening on port %d\n", port);
    _localPort = port;
    returnValue = true;

    _evThread->start();
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
  iax_event* event = 0;
  IAXCall* call = 0;
  while ( (event = iax_get_event(0)) != 0 ) {
    call = iaxFindCallBySession(event->session);
    if (call!=0) {
	iaxHandleCallEvent(event, call);
    } else if (event->session != 0 && event->session == _regSession) {
    	// in iaxclient, there is many session handling, here, only one
	iaxHandleRegReply(event);
    } else {
    	switch(e->etype) {
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
			_debug("Unknown event type: %d\n", event->type);
	}
    }
    iax_event_free(event);
  }
  // unlock mutex here

  //iaxRefreshRegistrations();

  // thread wait 5 millisecond
  _evThread->sleep(5);
}

bool
IAXVoIPLink::setRegister() 
{
  if (_regSession==0) {
    // lock

    _regSession = iax_session_new();

    if (!_regSession) {
      _debug("error when generating new session for register");
    } else {
      // refresh
      // last reg
    }

	// unlock
  }
  return false;
}

bool
IAXVoIPLink::setUnregister()
{
  return false;
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
  switch(event->type) {
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
    
    case IAX_EVENT_CNG:
    break;
    
    case IAX_EVENT_TIMEOUT:
    break;
    
    case IAX_EVENT_TRANSFER:
    break;
    
    default:
      _debug("Unknown event type: %d\n", event->type);
    
  }
}

void
IAXVoIPLink::iaxHandleRegReply(iax_event* event) 
{
  // use _regSession here, should be equal to event->session;
}
