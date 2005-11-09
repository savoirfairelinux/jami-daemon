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

#include <qstring.h>
#include <list>

#include "Account.hpp"
#include "Call.hpp"
#include "CallManager.hpp"
#include "Session.hpp"
#include "Requester.hpp"


Call::Call(const QString &sessionId,
	   const QString &accountId,
	   const QString &callId,
	   const QString &peer,
	   bool incomming)
  : mSessionId(sessionId)
  , mAccountId(accountId)
  , mId(callId)
  , mPeer(peer)
  , mIsIncomming(incomming)
{
  CallManager::instance().registerCall(*this);
}

Call::Call(const Session &session,
	   const Account &account,
	   const QString &callId,
	   const QString &peer,
	   bool incomming)
  : mSessionId(session.id())
  , mAccountId(account.id())
  , mId(callId)
  , mPeer(peer)
  , mIsIncomming(incomming)
{
  CallManager::instance().registerCall(*this);
}

bool
Call::isIncomming()
{return mIsIncomming;}

Request *
Call::answer() 
{
  mIsIncomming = false;
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "answer", args);
}

Request *
Call::hangup() 
{
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "hangup", args);
}

Request *
Call::cancel() 
{
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "cancel", args);
}

Request *
Call::hold() 
{
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "hold", args);
}

Request *
Call::unhold() 
{
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "unhold", args);
}

Request *
Call::refuse() 
{
  mIsIncomming = false;
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "refuse", args);
}

Request *
Call::notAvailable() 
{
  mIsIncomming = false;
  std::list< QString> args;
  args.push_back(mId);
  return Requester::instance().send(mSessionId, "notavailable", args);
}

Request *
Call::transfer(const QString &to) 
{
  std::list< QString> args;
  args.push_back(mId);
  args.push_back(to);
  return Requester::instance().send(mSessionId, "transfer", args);
}

Request *
Call::sendDtmf(char c) 
{
  std::list< QString> args;
  args.push_back(mId);
  QString s;
  s += c;
  args.push_back(s);
  return Requester::instance().send(mSessionId, "senddtmf", args);
}

