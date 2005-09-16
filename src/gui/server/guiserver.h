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
#include <stdexcept>
#include <list>
#include <cc++/socket.h>
#include <cc++/thread.h>
#include <map>

class GUIServer;
class Request 
{
public:
  Request(const std::string &sequenceId, const std::string &arg) : _sequenceId(sequenceId), _arg(arg) {}
  virtual ~Request() {}
  virtual std::string execute(GUIServer* gui) { return ""; }
  virtual std::string message(const std::string &code, const std::string &message);
  
protected:
  std::string _sequenceId;
  std::string _arg;
};


class RequestGlobalCall : public Request
{
public:
  RequestGlobalCall(const std::string &sequenceId, const std::string &arg) : Request(sequenceId,arg) {
    unsigned int spacePos = _arg.find(' ');
    if (spacePos == std::string::npos) {
      // only one argument, so it's must be the callid
      _callid = _arg;
    } else {
      _callid = _arg.substr(0, spacePos);
      _arg = _arg.substr(spacePos+1, _arg.size()-spacePos+1);
    }
  }
  virtual ~RequestGlobalCall() {}
  
protected:
  std::string _callid;
};

class RequestCall : public RequestGlobalCall {
public:
  RequestCall(const std::string &sequenceId, const std::string &arg) : RequestGlobalCall(sequenceId,arg) {
    // only one argument, so it's must be the account (that switch is JP fault)
    _account = _callid;
    _callid = "";
    unsigned int spacePos = _arg.find(' ');
    if (spacePos == std::string::npos) {
      _callid = _arg;
    } else {
      _callid = _arg.substr(0, spacePos);
      _destination = _arg.substr(spacePos+1, _arg.size()-spacePos+1);
    }
  }
  std::string execute(GUIServer *gui);

private:
  std::string _destination;
  std::string _account;
};

class RequestAnswer : public RequestGlobalCall {
public:
  RequestAnswer(const std::string &sequenceId, const std::string &arg) : RequestGlobalCall(sequenceId,arg) {}
};
class RequestRefuse : public RequestGlobalCall {
public:
  RequestRefuse(const std::string &sequenceId, const std::string &arg) : RequestGlobalCall(sequenceId,arg) {}
};
class RequestHold : public RequestGlobalCall {
public:
  RequestHold(const std::string &sequenceId, const std::string &arg) : RequestGlobalCall(sequenceId,arg) {}
};
class RequestUnhold : public RequestGlobalCall {
public:
  RequestUnhold(const std::string &sequenceId, const std::string &arg) : RequestGlobalCall(sequenceId,arg) {}
};
class RequestTransfer : public RequestGlobalCall {
public:
  RequestTransfer(const std::string &sequenceId, const std::string &arg) : RequestGlobalCall(sequenceId,arg) {}
};


class RequestGlobal : public Request
{
public:
  RequestGlobal(const std::string &sequenceId, const std::string &arg) : Request(sequenceId,arg) {}
  virtual ~RequestGlobal() {}
  virtual std::string execute(GUIServer *gui);

};

class RequestMute : public RequestGlobal {
public:
  RequestMute(const std::string &sequenceId, const std::string &arg) : RequestGlobal(sequenceId,arg) {}
};
class RequestUnmute : public RequestGlobal {
public:
  RequestUnmute(const std::string &sequenceId, const std::string &arg) : RequestGlobal(sequenceId,arg) {}
};
class RequestQuit : public RequestGlobal {
public:
  RequestQuit(const std::string &sequenceId, const std::string &arg) : RequestGlobal(sequenceId,arg) {}
};



class RequestSyntaxError : public Request 
{
public:
  RequestSyntaxError(const std::string &sequenceId, const std::string &arg) : Request(sequenceId, arg) {}
  ~RequestSyntaxError() {}
  std::string execute(GUIServer *gui) {
    return message("501", "Syntax Error");
  }
};

class RequestCreatorBase
{
public:
  virtual Request *create(const std::string &sequenceId, const std::string &arg) = 0;
  virtual RequestCreatorBase *clone() = 0;
};

template< typename T >
class RequestCreator : public RequestCreatorBase
{
public:
  virtual Request *create(const std::string &sequenceId, const std::string &arg)
  {
    return new T(sequenceId, arg);
  }

  virtual RequestCreatorBase *clone()
  {
    return new RequestCreator< T >();
  }
};


class RequestFactory
{
public:
  Request *create(const std::string &requestLine)
  {
    std::string requestName;
    std::string sequenceId="seq0";
    std::string arguments;
    
    unsigned int spacePos = requestLine.find(' ');
    // we find a spacePos
    if (spacePos != std::string::npos) {
      /*
      012345678901234
      call seq1 cdddd
      spacePos  = 4
      spacePos2 = 9
      0 for 4  = 0 for spacePos
      5 for 4  = (spacePos+1 for spacePos2-spacePos-1)
      10 for 5 = (spacePos2+1 for size - spacePos2+1)
      */
      requestName = requestLine.substr(0, spacePos);
      
      unsigned int spacePos2 = requestLine.find(' ', spacePos+1);
      if (spacePos2 == std::string::npos) {
        // command that end with a sequence number
        sequenceId = requestLine.substr(spacePos+1, requestLine.size()-spacePos+1);
      } else {
        sequenceId = requestLine.substr(spacePos+1, spacePos2-spacePos-1);
        arguments = requestLine.substr(spacePos2+1, requestLine.size()-spacePos2+1);
      }
    } else {
      requestName = "syntaxerror";
    }
    
    return create(requestName, sequenceId, arguments);
  }
  
  Request *create(
    const std::string &requestname, 
    const std::string &sequenceId, 
    const std::string &arg)
  {
    std::map< std::string, RequestCreatorBase * >::iterator pos = mRequests.find(requestname);
    if(pos == mRequests.end()) {
      pos = mRequests.find("syntaxerror");
      if(pos == mRequests.end()) {
        throw std::runtime_error("there's no request of that name");
      }
    }
    
    return pos->second->create(sequenceId, arg);
  }

  template< typename T >
  void registerRequest(const std::string &requestname)
  {
    std::map< std::string, RequestCreatorBase * >::iterator pos = 
      mRequests.find(requestname);
    if(pos != mRequests.end()) {
      delete pos->second;
      mRequests.erase(pos);
    }
    
    mRequests.insert(std::make_pair(requestname, new RequestCreator< T >()));
  }

 private:
  std::map< std::string, RequestCreatorBase * > mRequests;
};


class TCPSessionReader : public ost::TCPSession 
{
public:
  TCPSessionReader(ost::TCPSocket &server, GUIServer *gui) : 
    ost::TCPSession(server), 
    _gui(gui) {}

  void run();
      
private:
  GUIServer *_gui;
};

class TCPSessionWriter : public ost::TCPSession 
{
public:
  TCPSessionWriter(ost::TCPSocket &server, GUIServer *gui) : 
    ost::TCPSession(server), 
    _gui(gui) {}
    
  void run();
    
private:
  GUIServer *_gui;
};
  

class GUIServer : public GuiFramework {
public:
  // GUIServer constructor
  GUIServer();
  // GUIServer destructor
  ~GUIServer();
  
  // exec loop
  int exec(void);
  void pushRequestMessage(const std::string& request);
  Request *popRequest(void);
  void pushResponseMessage(const std::string& response);
  std::string popResponseMessage(void);
  
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
    
private:
  ost::TCPSession* sessionIn;
  ost::TCPSession* sessionOut;
  std::list<Request*> _requests; 
  std::list<std::string> _responses;
  RequestFactory *_factory;
  ost::Mutex _mutex;
};

#endif // __GUI_SERVER_H__
