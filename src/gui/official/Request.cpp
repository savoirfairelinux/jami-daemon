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

#include <qobject.h>
#include <sstream>

#include "globals.h"
#include "DebugOutput.hpp"
#include "CallManager.hpp"
#include "Request.hpp"
#include "Requester.hpp"

Request::Request(const QString &sequenceId,
		 const QString &command,
		 const std::list< QString > &args)
  : mSequenceId(sequenceId)
  , mCommand(command)
  , mArgs(args)
{}

std::list< QString >
Request::parseArgs(const QString &message)
{
  std::istringstream stream(message);
  std::string s;
  std::list< QString > args;
  while(stream.good()) {
    stream >> s;
    args.push_back(s);
  }

  return args;
}

void
Request::onError(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("Received an error:\n  "
					 "Code: %1\n  "
					 "SequenceID: %2\n  Message: %3\n")
    .arg(code)
    .arg(mSequenceId)
    .arg(message);
}

void
Request::onEntry(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("Received a temp info:\n  "
					 "Code: %1\n  "
					 "SequenceID: %2\n  "
					 "Message: %3\n")
    .arg(code)
    .arg(mSequenceId)
    .arg(message);
}

void
Request::onSuccess(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("Received a success info:\n  "
					 "Code: %1\n  "
					 "SequenceID: %2\n  "
					 "Message: %3\n")
    .arg(code)
    .arg(mSequenceId)
    .arg(message);
}

QString
Request::toString()
{
  QString output = mCommand + " " + mSequenceId;
  for(std::list< QString >::const_iterator pos = mArgs.begin();
      pos != mArgs.end();
      pos++) {
    output += " " + (*pos);
  }
  output += "\n";

  return output;
}


CallRelatedRequest::CallRelatedRequest(const QString &sequenceId,
			 const QString &command,
			 const std::list< QString > &args)
  : Request(sequenceId, command, args)
{}

void
CallRelatedRequest::onError(const QString &code, const QString &message)
{
  onError(CallManager::instance().getCall(mCallId), 
	  code, 
	  message);
}

void
CallRelatedRequest::onError(Call, const QString &, const QString &)
{}

void
CallRelatedRequest::onEntry(const QString &code, const QString &message)
{
  onEntry(CallManager::instance().getCall(mCallId),
	  code, 
	  message);
}

void
CallRelatedRequest::onEntry(Call, const QString &, const QString &)
{}

void
CallRelatedRequest::onSuccess(const QString &code, const QString &message)
{
  onSuccess(CallManager::instance().getCall(mCallId),
	    code, 
	    message);
}

void
CallRelatedRequest::onSuccess(Call, const QString &, const QString &)
{}

AccountRequest::AccountRequest(const QString &sequenceId,
			 const QString &command,
			 const std::list< QString > &args)
  : Request(sequenceId, command, args)
  , mAccountId(*args.begin())
{}

void
AccountRequest::onError(const QString &code, const QString &message)
{
  onError(Account(Requester::instance().getSessionIdFromSequenceId(getSequenceId()), 
	       mAccountId), 
	  code, 
	  message);
}

void
AccountRequest::onError(Account, const QString &, const QString &)
{}

void
AccountRequest::onEntry(const QString &code, const QString &message)
{
  onEntry(Account(Requester::instance().getSessionIdFromSequenceId(getSequenceId()), 
	       mAccountId), 
	  code, 
	  message);
}

void
AccountRequest::onEntry(Account, const QString &, const QString &)
{}

void
AccountRequest::onSuccess(const QString &code, const QString &message)
{
  onSuccess(Account(Requester::instance().getSessionIdFromSequenceId(getSequenceId()), 
		 mAccountId), 
	    code, 
	    message);
}

void
AccountRequest::onSuccess(Account, const QString &, const QString &)
{}

