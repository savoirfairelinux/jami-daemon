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

#include <iostream>
#include <stdexcept>
#include <sstream>

#include "globals.h"
#include "RequesterImpl.hpp"
#include "SessionIO.hpp"

RequesterImpl::RequesterImpl()
  : mCallIdCount(0)
  , mSessionIdCount(0)
  , mSequenceIdCount(0)
{}

SessionIO *
RequesterImpl::getSessionIO(const QString &sessionId)
{
  std::map< QString, SessionIO * >::iterator pos = mSessions.find(sessionId);
  if(pos == mSessions.end()) {
    throw std::runtime_error("The session is not valid.");
  }

  return (*pos).second;
}

QString 
RequesterImpl::send(const QString &sessionId,
		    const QString &command,
		    const std::list< QString > &args)
{
  // We retreive the internal of a session.
  SessionIO *session = getSessionIO(sessionId);

  // We ask the factory to create the request.
  QString sequenceId = generateSequenceId();
  Request *request = mRequestFactory.create(command, sequenceId, args);

  registerRequest(sessionId, sequenceId, request);
  session->send(request->toString());
  
  return sequenceId;
}

void
RequesterImpl::registerRequest(const QString &sessionId, 
			       const QString &sequenceId,
			       Request *request)
{
  if(mRequests.find(sequenceId) != mRequests.end()) {
    throw std::logic_error("Registering an already know sequence ID");
  }

  mRequests.insert(std::make_pair(sequenceId, request));
  mSequenceToSession.insert(std::make_pair(sequenceId, sessionId));
}


void
RequesterImpl::registerSession(const QString &id,
			       SessionIO *s)
{
  if(mSessions.find(id) != mSessions.end()) {
    throw std::logic_error("Registering an already know Session ID");
  }

  mSessions.insert(std::make_pair(id, s));
}

void
RequesterImpl::connect(const QString &id)
{
  std::map< QString, SessionIO * >::iterator pos = mSessions.find(id);
  if(pos == mSessions.end()) {
    throw std::logic_error("Trying to connect an unknown session.");
  }

  pos->second->connect();
}


int
RequesterImpl::getCodeCategory(const QString &code)
{
  int c;
  std::istringstream s(code.toStdString());
  s >> c;
  return c / 100;
}

void
RequesterImpl::receiveAnswer(const QString &answer)
{
  std::string code;
  std::string seq;
  std::string message;
  std::istringstream s(answer.toStdString());
  s >> code >> seq;
  getline(s, message);
  message.erase(0, 1);
  receiveAnswer(QString::fromStdString(code), 
		QString::fromStdString(seq),
		QString::fromStdString(message));
}


void
RequesterImpl::receiveAnswer(const QString &code, 
			     const QString &sequence, 
			     const QString &message)
{
  int c = getCodeCategory(code);

  std::map< QString, Request * >::iterator pos;
  pos = mRequests.find(sequence);
  if(pos == mRequests.end()) {
    _debug("Requester: We received an answer with an unknown sequence.\n");
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

QString
RequesterImpl::generateCallId()
{
  std::ostringstream id;
  id << "cCallID:" << mCallIdCount;
  mCallIdCount++;
  return QString::fromStdString(id.str());
}

QString
RequesterImpl::generateSessionId()
{
  std::ostringstream id;
  id << "cSessionID:" << mSessionIdCount;
  mSessionIdCount++;
  return QString::fromStdString(id.str());
}

QString
RequesterImpl::generateSequenceId()
{
  std::ostringstream id;
  id << "cSequenceID:" << mSequenceIdCount;
  mSequenceIdCount++;
  return QString::fromStdString(id.str());
}

void
RequesterImpl::inputIsDown(const QString &sessionId)
{
  std::map< QString, SessionIO * >::iterator pos;
  pos = mSessions.find(sessionId);
  if(pos == mSessions.end()) {
    // we will not thow an exception, but this is 
    // a logic error
    _debug("Requester: SessionIO input for session %s is down, "
	   "but we don't have that session.\n",
	   sessionId.toStdString().c_str());
  }
}
