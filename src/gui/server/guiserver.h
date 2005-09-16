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
  Request(const std::string &sequenceId) : _sequenceId(sequenceId) {}
  virtual ~Request() {}
  virtual std::string execute() { return ""; }
  virtual std::string error(const std::string &code, const std::string &error) 
  {
    std::string returnError = code + " " + _sequenceId + " " + error;
    return returnError;
  }
protected:
  std::string _sequenceId;
};


class RequestCall : public Request
{
public:
  RequestCall(const std::string &sequenceId, const std::string &arg) : Request(sequenceId), _arg(arg) {}
  virtual ~RequestCall() {}
  virtual std::string execute() 
  {
    std::string returnOK = std::string("200 ") + _sequenceId + " OK";
    return returnOK; 
  }
  std::string _arg;
};

class RequestAnswer : public RequestCall {
public:
  RequestAnswer(const std::string &sequenceId, const std::string &arg) : RequestCall(sequenceId,arg) {}
};
class RequestRefuse : public RequestCall {
public:
  RequestRefuse(const std::string &sequenceId, const std::string &arg) : RequestCall(sequenceId,arg) {}
};
class RequestHold : public RequestCall {
public:
  RequestHold(const std::string &sequenceId, const std::string &arg) : RequestCall(sequenceId,arg) {}
};
class RequestUnhold : public RequestCall {
public:
  RequestUnhold(const std::string &sequenceId, const std::string &arg) : RequestCall(sequenceId,arg) {}
};
class RequestTransfer : public RequestCall {
public:
  RequestTransfer(const std::string &sequenceId, const std::string &arg) : RequestCall(sequenceId,arg) {}
};


class RequestGlobal : public Request
{
public:
  RequestGlobal(const std::string &sequenceId) : Request(sequenceId) {}
  virtual ~RequestGlobal() {}
  virtual std::string execute() 
  {
    std::string returnOK = std::string("200 ") + _sequenceId + " OK";
    return returnOK; 
  }
};

class RequestMute : public RequestGlobal {
public:
  RequestMute(const std::string &sequenceId) : RequestGlobal(sequenceId) {}
};
class RequestUnmute : public RequestGlobal {
public:
  RequestUnmute(const std::string &sequenceId) : RequestGlobal(sequenceId) {}
};
class RequestQuit : public RequestGlobal {
public:
  RequestQuit(const std::string &sequenceId) : RequestGlobal(sequenceId) {}
};



class RequestSyntaxError : public Request 
{
public:
  RequestSyntaxError(const std::string &sequenceId = "seq0") : Request(sequenceId) {}
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
      std::string cmd = requestLine.substr(0, spacePos);
      
      unsigned int spacePos2 = requestLine.find(' ', spacePos+1);
      if (spacePos2 == std::string::npos) {
        // command that end with a sequence number
        std::string seq = requestLine.substr(spacePos+1, requestLine.size()-spacePos+1);
        
        if (cmd == CMD_MUTE) {
          return new RequestMute(seq);  
        } else if (cmd == CMD_UNMUTE) {
          return new RequestUnmute(seq);  
        } else if (cmd == CMD_QUIT) {
          return new RequestQuit(seq);  
        } else {
          return new RequestSyntaxError(seq);
        }
      } else {
        std::string seq = requestLine.substr(spacePos+1, spacePos2-spacePos-1);
        std::string arg = requestLine.substr(spacePos2+1, requestLine.size()-spacePos2+1);
        
        // command with args
        if (cmd == CMD_CALL) {
          return new RequestCall(seq, arg);
        } else if (cmd == CMD_ANWSER) {
          return new RequestAnswer(seq, arg);  
        } else if (cmd == CMD_REFUSE) {
          return new RequestRefuse(seq, arg);  
        } else if (cmd == CMD_HOLD) {
          return new RequestHold(seq, arg);  
        } else if (cmd == CMD_UNHOLD) {
          return new RequestUnhold(seq, arg);  
        } else if (cmd == CMD_TRANSFER) {
          return new RequestTransfer(seq, arg);  
        } else {
          return new RequestSyntaxError(seq);
        }
      }
    }
    std::cout << "RequestLine: " << requestLine << std::endl;
    return new RequestSyntaxError();
  }
  static const std::string CMD_CALL;
  static const std::string CMD_ANWSER;
  static const std::string CMD_REFUSE;
  static const std::string CMD_HOLD;
  static const std::string CMD_UNHOLD;
  static const std::string CMD_TRANSFER;
  static const std::string CMD_MUTE;
  static const std::string CMD_UNMUTE;
  static const std::string CMD_QUIT;
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
