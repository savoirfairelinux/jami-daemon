/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
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
#ifndef __GUI_SERVER_H__
#define __GUI_SERVER_H__

#include "../guiframework.h"
#include <string>
#include <iostream>
#include <list>
#include <map>
#include <cc++/socket.h>
#include <cc++/thread.h>

#include "subcall.h"
#include "requestfactory.h"
#include "ObjectPool.hpp"

class GUIServer;
class TCPSessionIO : public ost::TCPSession 
{
public:
  TCPSessionIO(ost::TCPSocket &server, GUIServer *gui) : 
    ost::TCPSession(server), 
    _gui(gui) {}

  void run();
  void push(const std::string &response) {
    _outputPool.push(response);
  }
  
private:
  GUIServer *_gui;
  ObjectPool<std::string> _outputPool;
};

typedef std::map<short, SubCall> CallMap;
class ResponseMessage;
class GUIServer : public GuiFramework {
public:
  // GUIServer constructor
  GUIServer();
  // GUIServer destructor
  ~GUIServer();
  
  // exec loop
  int exec(void);
  //void handleExecutedRequest(Request* request, const 
  void pushRequestMessage(const std::string& request);
  Request *popRequest(void);
  void pushResponseMessage(const ResponseMessage& response);
  void handleExecutedRequest(Request * const request, const ResponseMessage& response);

  void insertSubCall(short id, SubCall& subCall);
  void removeSubCall(short id);
  std::string getSequenceIdFromId(short id);
  short getIdFromCallId(const std::string& callId);

  // Reimplementation of virtual functions
	virtual int incomingCall (short id);
	virtual void peerAnsweredCall (short id);
	virtual int peerRingingCall (short id);
	virtual int peerHungupCall (short id);
	virtual void displayTextMessage (short id, const std::string& message);
	virtual void displayErrorText (short id, const std::string& message);
	virtual void displayError (const std::string& error);
	virtual void displayStatus (const std::string& status);
	virtual void displayContext (short id);
	virtual std::string getRingtoneFile (void);
	virtual void setup (void);
	virtual int selectedCall (void);
	virtual bool isCurrentId (short);
	virtual void startVoiceMessageNotification (void);
	virtual void stopVoiceMessageNotification (void);  

  int outgoingCall (const std::string& to) {return GuiFramework::outgoingCall(to);}
  void hangup(const std::string& callId);
    
private:
  TCPSessionIO* _sessionIO;

  /**
   * This callMap is necessary because
   * ManagerImpl use callid-int
   * and the client use a  callid-string
   * and also a sequence number
   */
  CallMap _callMap;
  // Incoming requests not executed
  ObjectPool<Request*> _requests;
  // Requests executed but waiting for a final response
  std::map<std::string, Request*> _waitingRequests;

  RequestFactory _factory;
  ost::Mutex _mutex;
};

#endif // __GUI_SERVER_H__
