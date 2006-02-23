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
#include <qstringlist.h>
#include <qurl.h>

#include "globals.h"
#include "DebugOutput.hpp"
#include "CallManager.hpp"
#include "Request.hpp"
#include "Requester.hpp"
#include "Url.hpp"

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
  QStringList list(QStringList::split(" ", message));
  std::list< QString > args;
  for(QStringList::Iterator it = list.begin(); it != list.end(); ++it) {
    QString qs(*it);
    Url::decode(qs);
    args.push_back(qs);
  }

  return args;
}

void
Request::onError(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("Received an error:        " 
					 "Code/SeqID: %1/%2\t"
					 "Message: %3\n")
    .arg(code)
    .arg(mSequenceId)
    .arg(message);

  emit error(message, code);
}

void
Request::onEntry(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("Received a temp info:     "
					 "Code/SeqID: %1/%2\t"
					 "Message: %3\n")
    .arg(code)
    .arg(mSequenceId)
    .arg(message);

  emit entry(message, code);

  // This is bad code, I know. I need to find a better way.
  std::list< QString > args = parseArgs(message);
  QString arg1, arg2, arg3, arg4, arg5;
  if(args.size() >= 1) {
    arg1 = *args.begin();
    args.pop_front();
  }
  if(args.size() >= 1) {
    arg2 = *args.begin();
    args.pop_front();
  }
  if(args.size() >= 1) {
    arg3 = *args.begin();
    args.pop_front();
  }
  if(args.size() >= 1) {
    arg4 = *args.begin();
    args.pop_front();
  }
  if(args.size() >= 1) {
    arg5 = *args.begin();
    args.pop_front();
  }
  emit parsedEntry(arg1, arg2, arg3, arg4, arg5);
}

void
Request::onSuccess(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("Received a success info:  "
					 "Code/SeqID: %1/%2\t"
					 "Message: %3\n")
    .arg(code)
    .arg(mSequenceId)
    .arg(message);

  emit success(message, code);
}

QString
Request::toString()
{
  QString output = mCommand + " " + mSequenceId;
  for(std::list< QString >::const_iterator pos = mArgs.begin();
      pos != mArgs.end();
      pos++) {
    QString ostring(*pos);
    QUrl::encode(ostring);
    output += " " + ostring;
  }
  output += "\n";

  return output;
}


CallRelatedRequest::CallRelatedRequest(const QString &sequenceId,
			 const QString &command,
			 const std::list< QString > &args)
  : Request(sequenceId, command, args)
{
  if(args.begin() != args.end()) {
    mCallId = *args.begin();
  }
}

void
CallRelatedRequest::onError(const QString &code, const QString &message)
{
  if(CallManager::instance().exist(mCallId)) {
    onError(CallManager::instance().getCall(mCallId), 
	    code, 
	    message);
  }
  else {
    DebugOutput::instance() << QObject::tr("CallRelatedRequest: Trying to retreive an unregistred call (%1)\n").arg(mCallId);
  }
}

void
CallRelatedRequest::onError(Call, const QString &, const QString &)
{}

void
CallRelatedRequest::onEntry(const QString &code, const QString &message)
{
  if(CallManager::instance().exist(mCallId)) {
    onEntry(CallManager::instance().getCall(mCallId),
	    code, 
	    message);
  }
  else {
    DebugOutput::instance() << QObject::tr("CallRelatedRequest: Trying to retreive an unregistred call (%1)\n").arg(mCallId);
  }
}

void
CallRelatedRequest::onEntry(Call, const QString &, const QString &)
{}

void
CallRelatedRequest::onSuccess(const QString &code, const QString &message)
{
  if(CallManager::instance().exist(mCallId)) {
    onSuccess(CallManager::instance().getCall(mCallId),
	      code, 
	      message);
  }
  else {
    DebugOutput::instance() << QObject::tr("CallRelatedRequest: Trying to retreive an unregistred call (%1)\n").arg(mCallId);
  }
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

