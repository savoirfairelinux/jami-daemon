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

#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <string>
#include "responsemessage.h"

/**
Request are received from the client
and execute on the server
Request execution always return a ResponseMessage
@author Yan Morin
*/
class GUIServer;
class Request 
{
public:
  Request(const std::string &sequenceId, const std::string &arg) : _sequenceId(sequenceId), _arg(arg) {}
  virtual ~Request() {}
  virtual ResponseMessage execute(GUIServer& gui) = 0;
  ResponseMessage message(const std::string &code, const std::string &message) {
    ResponseMessage response(_sequenceId, code, message);
    return response;
  }
  std::string sequenceId () const { return _sequenceId; }
  
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
      // only one argument, so it's must be the callId
      _callId = _arg;
    } else {
      _callId = _arg.substr(0, spacePos);
      _arg = _arg.substr(spacePos+1, _arg.size()-spacePos+1);
    }
  }
  virtual ~RequestGlobalCall() {}
  virtual ResponseMessage execute(GUIServer& gui) { return message("200","OK"); }

protected:
  std::string _callId;
};

class RequestCall : public RequestGlobalCall {
public:
  RequestCall(const std::string &sequenceId, const std::string &arg) : RequestGlobalCall(sequenceId,arg) {
    // only one argument, so it's must be the account (that switch is JP fault)
    _account = _callId;
    _callId = "";
    unsigned int spacePos = _arg.find(' ');
    if (spacePos == std::string::npos) {
      _callId = _arg;
    } else {
      _callId = _arg.substr(0, spacePos);
      _destination = _arg.substr(spacePos+1, _arg.size()-spacePos+1);
    }
  }
  virtual ResponseMessage execute(GUIServer& gui);

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
  virtual ResponseMessage execute(GUIServer& gui) { return message("200","OK"); }
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
  ResponseMessage execute(GUIServer& gui) {
    return message("501", "Syntax Error");
  }
};

#endif
