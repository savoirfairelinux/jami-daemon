/*
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
#include <list>
#include "responsemessage.h"

typedef std::list<std::string> TokenList;

/**
Request are received from the client
and execute on the server
Request execution always return a ResponseMessage
@author Yan Morin
*/
class RequestConstructorException {
};

class Request 
{
public:
  Request(const std::string &sequenceId, const TokenList& argList) : _sequenceId(sequenceId), _argList(argList) {}
  virtual ~Request() {}
  virtual ResponseMessage execute() = 0;
  ResponseMessage message(const std::string &code, const std::string &aMessage) {
    ResponseMessage response(code, _sequenceId, aMessage);
    return response;
  }
  ResponseMessage message(const std::string &code, TokenList& arg) {
    ResponseMessage response(code, _sequenceId, arg);
    return response;
  }
  std::string getSequenceId () const { return _sequenceId; }
  
protected:
  std::string _sequenceId;
  TokenList _argList;
};


class RequestCall : public Request {
public:
  RequestCall(const std::string &sequenceId, const TokenList& argList) : Request(sequenceId,argList) {
    TokenList::iterator iter = _argList.begin();
    // check for the callid
    bool argsAreValid = false;
    // Args are: account callid destination
    //           acc1000 c10345 sip:test@test.com
    if (iter != _argList.end() && iter->length()!=0) {
      _account = *iter;
      _argList.pop_front();
      iter = _argList.begin();
      if (iter != _argList.end() && iter->length() != 0) {
        _callId = *iter;
        // last arg is the destination
        iter++;
        if (iter != _argList.end()) {
          _destination = *iter;
          argsAreValid = true;
        }
      }
    }
    if (!argsAreValid) {
      throw RequestConstructorException();
    }
  }
  virtual ResponseMessage execute();

private:
  std::string _callId;
  std::string _destination;
  std::string _account;
};

/**
 * Class for Text Message
 * message <seq> <acc> <to> <message>
 */
class RequestTextMessage : public Request {
public:
  RequestTextMessage(const std::string &sequenceId, const TokenList& argList) : Request(sequenceId,argList) {
    TokenList::iterator iter = _argList.begin();
    // check for the callid
    bool argsAreValid = false;
    if (iter != _argList.end() && iter->length()!=0) {
      _account = *iter;
      _argList.pop_front();
      iter = _argList.begin();
      if (iter != _argList.end() && iter->length() != 0) {
        _destination = *iter;
        // last arg is the destination
        iter++;
        if (iter != _argList.end()) {
          _message = *iter;
          argsAreValid = true;
        }
      }
    }
    if (!argsAreValid) {
      throw RequestConstructorException();
    }
  }
  virtual ResponseMessage execute();

private:
  std::string _account;
  std::string _destination;
  std::string _message;
};


class RequestGlobalCall : public Request
{
public:
  RequestGlobalCall(const std::string &sequenceId, const TokenList& argList) : Request(sequenceId, argList) {
    TokenList::iterator iter = _argList.begin();

    if (iter != _argList.end() && iter->length() != 0 ) {
      _callId = *iter;
      _argList.pop_front();
    } else {
      throw RequestConstructorException();
    }
  }
  virtual ~RequestGlobalCall() {}
  virtual ResponseMessage execute() = 0;

protected:
  std::string _callId;
};

class RequestAnswer : public RequestGlobalCall {
public:
  RequestAnswer(const std::string &sequenceId, const TokenList& argList) : RequestGlobalCall(sequenceId,argList) {}
  ResponseMessage execute();
};
class RequestRefuse : public RequestGlobalCall {
public:
  RequestRefuse(const std::string &sequenceId, const TokenList& argList) : RequestGlobalCall(sequenceId,argList) {}
  ResponseMessage execute();
};
class RequestHold : public RequestGlobalCall {
public:
  RequestHold(const std::string &sequenceId, const TokenList& argList) : RequestGlobalCall(sequenceId,argList) {}
  ResponseMessage execute();
};
class RequestUnhold : public RequestGlobalCall {
public:
  RequestUnhold(const std::string &sequenceId, const TokenList& argList) : RequestGlobalCall(sequenceId,argList) {}
  ResponseMessage execute();
};
class RequestTransfer : public RequestGlobalCall {
public:
  RequestTransfer(const std::string &sequenceId, const TokenList& argList);
  ResponseMessage execute();
private:
  std::string _destination;
};
class RequestHangup : public RequestGlobalCall {
public:
  RequestHangup(const std::string &sequenceId, const TokenList& argList) : RequestGlobalCall(sequenceId,argList) {}
  ResponseMessage execute();
};

class RequestDTMF : public RequestGlobalCall {
public:
  RequestDTMF(const std::string &sequenceId, 
    const TokenList& argList);

  ResponseMessage execute();
private:
  std::string _dtmfKey;
};


class RequestGlobal : public Request
{
public:
  RequestGlobal(const std::string &sequenceId, const TokenList& argList) : Request(sequenceId,argList) {}
  virtual ~RequestGlobal() {}
  virtual ResponseMessage execute() = 0;
};

class RequestMute : public RequestGlobal {
public:
  RequestMute(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};
class RequestUnmute : public RequestGlobal {
public:
  RequestUnmute(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};
class RequestVersion : public RequestGlobal {
public:
  RequestVersion(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};
class RequestQuit : public RequestGlobal {
public:
  RequestQuit(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};
class RequestStop : public RequestGlobal {
public:
  RequestStop(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};
class RequestHangupAll : public RequestGlobal {
public:
  RequestHangupAll(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};

class RequestPlayDtmf : public RequestGlobal {
public:
  RequestPlayDtmf(const std::string &sequenceId, 
    const TokenList& argList);
  ResponseMessage execute();
private:
  std::string _dtmfKey;
};

class RequestPlayTone : public RequestGlobal {
public:
  RequestPlayTone(const std::string &sequenceId, 
    const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};

class RequestStopTone : public RequestGlobal {
public:
  RequestStopTone(const std::string &sequenceId, 
    const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};





class RequestSyntaxError : public Request 
{
public:
  RequestSyntaxError(const std::string &sequenceId, const TokenList& argList) : Request(sequenceId, argList) {}
  ~RequestSyntaxError() {}
  ResponseMessage execute() {
    return message("501", "Syntax Error");
  }
};

#endif
