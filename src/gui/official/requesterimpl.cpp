/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdexcept>
#include <sstream>

#include "requesterimpl.h"
#include "sessionio.h"

RequesterImpl::RequesterImpl()
  : mCallIdCount(0)
  , mSessionIdCount(0)
  , mSequenceIdCount(0)
{}

SessionIO *
RequesterImpl::getSessionIO(const std::string &sessionId)
{
  std::map< std::string, SessionIO * >::iterator pos = mSessions.find(sessionId);
  if(pos == mSessions.end()) {
    throw std::runtime_error("The session is not valid.");
  }

  return (*pos).second;
}

std::string 
RequesterImpl::send(const std::string &sessionId,
		    const std::string &command,
		    const std::list< std::string > &args)
{
  // We retreive the internal of a session.
  SessionIO *session = getSessionIO(sessionId);

  // We ask the factory to create the request.
  std::string sequenceId = generateSequenceId();
  Request *request = mRequestFactory.create(command, sequenceId, args);

  registerRequest(sessionId, sequenceId, request);
  session->send(request->toString());
  
  return sequenceId;
}

void
RequesterImpl::registerRequest(const std::string &sessionId, 
			       const std::string &sequenceId,
			       Request *request)
{
  if(mRequests.find(sequenceId) != mRequests.end()) {
    throw std::logic_error("Registering an already know sequence ID");
  }

  mRequests.insert(std::make_pair(sequenceId, request));
  mSequenceToSession.insert(std::make_pair(sequenceId, sessionId));
}


void
RequesterImpl::registerSession(const std::string &id,
			       SessionIO *s)
{
  if(mSessions.find(id) != mSessions.end()) {
    throw std::logic_error("Registering an already know Session ID");
  }

  mSessions.insert(std::make_pair(id, s));
  s->start();
}

int
RequesterImpl::getCodeCategory(const std::string &code)
{
  int c;
  std::istringstream s(code);
  s >> c;
  return c / 100;
}

void
RequesterImpl::receiveAnswer(const std::string &answer)
{
  std::string code;
  std::string seq;
  std::string message;
  std::istringstream s(answer);
  s >> code >> seq;
  getline(s, message);
  receiveAnswer(code, seq, message);
}


void
RequesterImpl::receiveAnswer(const std::string &code, 
			     const std::string &sequence, 
			     const std::string &message)
{
  int c = getCodeCategory(code);

  std::map< std::string, Request * >::iterator pos;
  pos = mRequests.find(sequence);
  if(pos == mRequests.end()) {
    std::cerr << "We received an answer with an unknown sequence" << std::endl;
    return;
  }

  if(c <= 1) {
    //Other answers will come for this request.
    pos->second->onEntry(code, message);
  }
  else{
    //This is the final answer of this request.
    if(c == 2) {
      pos->second->onSuccess(code, message);
    }
    else {
      pos->second->onError(code, message);
    }
    delete pos->second;
    mRequests.erase(pos);
  }
}	       

std::string
RequesterImpl::generateCallId()
{
  std::ostringstream id;
  id << "cCallID:" << mCallIdCount;
  mCallIdCount++;
  return id.str();
}

std::string
RequesterImpl::generateSessionId()
{
  std::ostringstream id;
  id << "cSessionID:" << mSessionIdCount;
  mSessionIdCount++;
  return id.str();
}

std::string
RequesterImpl::generateSequenceId()
{
  std::ostringstream id;
  id << "cSequenceID:" << mSequenceIdCount;
  mSequenceIdCount++;
  return id.str();
}

