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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <QString>
#include <list>

#include "Call.hpp"
#include "Session.hpp"
#include "Requester.hpp"


Call::Call(const QString &sessionId,
	   const QString &callId,
	   bool incomming)
  : mSessionId(sessionId)
  , mId(callId)
  , mIsIncomming(incomming)
{}

Call::Call(const Session &session,
	   const QString &callId,
	   bool incomming)
  : mSessionId(session.id())
  , mId(callId)
  , mIsIncomming(incomming)
{}

bool
Call::isIncomming()
{return mIsIncomming;}

QString
Call::call(const QString &to) 
{
  std::list< QString> args;
  args.push_back(mId);
  args.push_back(to);
  return Requester::instance().send(mSessionId, "call", args);
}

QString
Call::answer() 
{
  mIsIncomming = false;
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "answer", args);
}

QString
Call::hangup() 
{
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "hangup", args);
}

QString
Call::cancel() 
{
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "cancel", args);
}

QString
Call::hold() 
{
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "hold", args);
}

QString
Call::unhold() 
{
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "unhold", args);
}

QString
Call::refuse() 
{
  mIsIncomming = false;
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "refuse", args);
}

QString
Call::notAvailable() 
{
  mIsIncomming = false;
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "notavailable", args);
}

QString
Call::sendDtmf(char c) 
{
  std::list< QString> args;
  args.push_back(mId);
  QString s;
  s += c;
  args.push_back(s);
  return Requester::instance().send(mSessionId, "senddtmf", args);
}

