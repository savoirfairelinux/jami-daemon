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
#include <list>
#include <cc++/socket.h>
#include <map>

class Request 
{
public:
  Request(const std::string &cseq) { 
    _cseq = cseq;
  }
  virtual ~Request(){}
  virtual std::string execute();
  std::string error(const std::string &code, const std::string &error) {
    std::string returnError = code + " " + _cseq + " " + error;
    return returnError;
  }
private:
  std::string _cseq;
};


class RequestCall : public Request
{
public:
  RequestCall(const std::string &cseq) : Request(cseq) {}
  ~RequestCall() {}
  std::string execute() { return ""; }
};

class RequestSyntaxError : public Request 
{
public:
  RequestSyntaxError(const std::string &cseq = "seq0") : Request(cseq) {}
  ~RequestSyntaxError() {}
  std::string execute() {
    return error("501", "Syntax Error");
  }
};

class RequestFactory 
{
public:
  RequestFactory(){
  }
  ~RequestFactory(){
  }
  
  Request* createNewRequest(const std::string &requestLine) 
  {
    int spacePos = requestLine.find(' ');
    // we find a spacePos
    if ( spacePos != -1 ) {
      return new RequestSyntaxError();
    }
  }
};

class GUIServer : public GuiFramework {
public:
  // GUIServer constructor
  GUIServer();
  // GUIServer destructor
  ~GUIServer();
  
  // exec loop
  int exec(void);
  
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
  
private:
  ost::TCPStream* aServerStream;
  std::map<std::string, Request*> _requestMap; 
};

#endif // __GUI_SERVER_H__
