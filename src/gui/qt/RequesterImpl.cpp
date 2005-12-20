/*
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

#include <qtextstream.h>
#include <iostream>
#include <stdexcept>
#include <sstream>

#include "globals.h"
#include "DebugOutput.hpp"
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

Request *
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
  
  return request;
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

Request *
RequesterImpl::getRequest(const QString &sequenceId)
{ 
  Request *r = NULL;

  std::map< QString, Request * >::iterator pos = mRequests.find(sequenceId);
  if(pos == mRequests.end()) {
    r = pos->second;
  }

  return r;
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
  return code.toInt() / 100;
}

void
RequesterImpl::receiveAnswer(const QString &answer)
{
  QString a(answer);
  QString code;
  QString seq;
  QString message;

  QTextStream s(&a, IO_ReadOnly);
  s >> code >> seq;
  message = s.readLine();
  message.remove(0, 1);
  receiveAnswer(code, seq, message);
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
    DebugOutput::instance() << QObject::tr("Requester: We received an answer "
					   "with an unknown sequence (%1).\n")
      .arg(sequence);
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
  return QString("cCallID:%1").arg(mCallIdCount++);
}

QString
RequesterImpl::generateSessionId()
{
  return QString("cSessionID:%1").arg(mSequenceIdCount++);
}

QString
RequesterImpl::generateSequenceId()
{
  return QString("cSequenceID:%1").arg(mSequenceIdCount++);
}

void
RequesterImpl::inputIsDown(const QString &sessionId)
{
  std::map< QString, SessionIO * >::iterator pos;
  pos = mSessions.find(sessionId);
  if(pos == mSessions.end()) {
    // we will not thow an exception, but this is 
    // a logic error
    DebugOutput::instance() << QObject::tr("Requester: SessionIO input for session %1 is down, "
					   "but we don't have that session.\n")
      .arg(sessionId);
  }
}
