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

#include <iostream>
#include <memory> // for std::auto_ptr
#include <sstream>
#include <string>
#include <qstring.h>

#include "globals.h"
#include "CallManager.hpp"
#include "CallStatus.hpp"
#include "CallStatusFactory.hpp"
#include "ConfigurationManager.hpp"
#include "PhoneLine.hpp"
#include "PhoneLineLocker.hpp"
#include "PhoneLineManager.hpp"
#include "SFLRequest.hpp"

EventRequest::EventRequest(const QString &sequenceId,
			   const QString &command,
			   const std::list< QString > &args)
  : Request(sequenceId, command, args)
{}


void
EventRequest::onError(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("EventRequest error: (%1) %1\n")
    .arg(code)
    .arg(message);
  PhoneLineManager::instance().errorOnGetEvents(message);
}

void
EventRequest::onEntry(const QString &code, const QString &message)
{
  std::auto_ptr< Event > 
    e(EventFactory::instance().create(code, Request::parseArgs(message)));
  e->execute();
}

void
EventRequest::onSuccess(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("EventRequest success: (%1) %1\n")
    .arg(code)
    .arg(message);
  PhoneLineManager::instance().connect();
}

CallStatusRequest::CallStatusRequest(const QString &sequenceId,
				     const QString &command,
				     const std::list< QString > &args)
  : Request(sequenceId, command, args)
{}


void
CallStatusRequest::onError(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("CallStatusRequest error: (%1) %1\n")
    .arg(code)
    .arg(message);
  PhoneLineManager::instance().errorOnCallStatus(message);
}

void
CallStatusRequest::onEntry(const QString &code, const QString &message)
{
  std::auto_ptr< Event > 
    e(EventFactory::instance().create(code, Request::parseArgs(message)));
  e->execute();
}

void
CallStatusRequest::onSuccess(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("CallStatusRequest success: (%1) %1\n")
    .arg(code)
    .arg(message);
  if(code == "206") {
    std::list< QString > args = Request::parseArgs(message);
    if(args.size() >= 2) {
      PhoneLineManager::instance().selectLine(*args.begin(), true);
    }
    else {
      DebugOutput::instance() << QObject::tr("CallStatusRequest Error: cannot get current line.\n");
    }
  }
  PhoneLineManager::instance().handleEvents();
}


PermanentRequest::PermanentRequest(const QString &sequenceId,
			 const QString &command,
			 const std::list< QString > &args)
  : CallRelatedRequest(sequenceId, command, args)
{}

void
PermanentRequest::onError(Call call, 
		     const QString &, 
		     const QString &message)
{
  DebugOutput::instance() << QObject::tr("PermanentRequest: Error: %1").arg(toString());
  PhoneLine *line = PhoneLineManager::instance().getLine(call);
  if(line) {
    PhoneLineLocker guard(line, false);
    line->error(message);
    line->setAction("");
  }
  else {
    DebugOutput::instance() << 
      QObject::tr("We received an error on a call "
		  "that doesn't have a phone line (%1).\n")
      .arg(call.id());
  }
}

void
PermanentRequest::onEntry(Call call, 
			    const QString &, 
			    const QString &message)
{
  PhoneLine *line = PhoneLineManager::instance().getLine(call);
  if(line) {
    PhoneLineLocker guard(line, false);
    line->setLineStatus(message);
    line->setAction("");
  }
  else {
    DebugOutput::instance() << 
      QObject::tr("We received a status on a call related request "
		  "that doesn't have a phone line (%1).\n")
      .arg(call.id());
  }
}

void
PermanentRequest::onSuccess(Call call, 
			    const QString &, 
			    const QString &message)
{
  PhoneLine *line = PhoneLineManager::instance().getLine(call);
  if(line) {
    PhoneLineLocker guard(line, false);
    line->setLineStatus(message);
    line->setAction("");
  }
  else {
    DebugOutput::instance() << 
      QObject::tr("We received a success on a call related request "
		  "that doesn't have a phone line (%1).\n")
      .arg(call.id());
  }
}

TemporaryRequest::TemporaryRequest(const QString &sequenceId,
				   const QString &command,
				   const std::list< QString > &args)
  : CallRelatedRequest(sequenceId, command, args)
{}

void
TemporaryRequest::onError(Call call, 
			  const QString &code, 
			  const QString &message)
{
  onSuccess(call, code, message);
}

void
TemporaryRequest::onEntry(Call call, 
			  const QString &code,
			  const QString &message)
{
  onSuccess(call, code, message);
}

void
TemporaryRequest::onSuccess(Call call, 
			    const QString &, 
			    const QString &message)
{
  PhoneLine *line = PhoneLineManager::instance().getLine(call);
  if(line) {
    PhoneLineLocker guard(line, false);
    line->setTempAction(message);
  }
  else {
    DebugOutput::instance() << 
      QObject::tr("We received an answer on a temporary call "
		  "related request that doesn't have a phone "
		  "line (%1).\n")
      .arg(call.id());
  }
}

CallRequest::CallRequest(const QString &sequenceId,
			 const QString &command,
			 const std::list< QString > &args)
  : AccountRequest(sequenceId, command, args)
{
  
  std::list< QString >::const_iterator pos = args.begin();
  pos++;
  mCallId = *pos;
}

void
CallRequest::onError(Account, 
		     const QString &, 
		     const QString &message)
{
  if(CallManager::instance().exist(mCallId)) {
    PhoneLine *line = 
      PhoneLineManager::instance().getLine(CallManager::instance().getCall(mCallId));
    if(line) {
      PhoneLineLocker guard(line, false);
      line->error(message);
    }
    else {
      DebugOutput::instance() << 
	QObject::tr("We received an error on a call "
		    "that doesn't have a phone line (%1).\n")
	.arg(mCallId);
    }
  }
  else {
    DebugOutput::instance() << QObject::tr("CallRequest: Trying to retreive an unregistred call (%1)\n").arg(mCallId);
  }
}

void
CallRequest::onEntry(Account, 
		     const QString &, 
		     const QString &message)
{
  if(CallManager::instance().exist(mCallId)) {
    PhoneLine *line = 
      PhoneLineManager::instance().getLine(CallManager::instance().getCall(mCallId));
    if(line) {
      PhoneLineLocker guard(line, false);
      line->setLineStatus(message);
    }
    else {
      DebugOutput::instance() << 
	QObject::tr("We received a status on a call related request "
		    "that doesn't have a phone line (%1).\n")
	.arg(mCallId);
    }
  }
  else {
    DebugOutput::instance() << QObject::tr("CallRequest: Trying to retreive an unregistred call (%1)\n").arg(mCallId);
  }
}

void
CallRequest::onSuccess(Account, 
		       const QString &, 
		       const QString &message)
{
  if(CallManager::instance().exist(mCallId)) {
    PhoneLine *line = 
      PhoneLineManager::instance().getLine(CallManager::instance().getCall(mCallId));
    if(line) {
      PhoneLineLocker guard(line, false);
      line->setLineStatus(message);
    }
    else {
      DebugOutput::instance() <<
	QObject::tr("We received a success on a call related request "
		    "that doesn't have a phone line (%1).\n")
	.arg(mCallId);
    }
  }
  else {
    DebugOutput::instance() << QObject::tr("CallRequest: Trying to retreive an unregistred call (%1)\n").arg(mCallId);
  }
}



ConfigGetAllRequest::ConfigGetAllRequest(const QString &sequenceId,
					 const QString &command,
					 const std::list< QString > &args)
  : Request(sequenceId, command, args)
{}


void
ConfigGetAllRequest::onError(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("ConfigGetAllRequest error: (%1) %1\n")
    .arg(code)
    .arg(message);
}

void
ConfigGetAllRequest::onEntry(const QString &, const QString &message)
{
  std::list< QString > args = Request::parseArgs(message);
  if(args.size() >= 3) {
    QString section, variable, type, def, val;
    section = *args.begin();
    args.pop_front();
    variable = *args.begin();
    args.pop_front();
    type = *args.begin();
    args.pop_front();
    if(args.size() >= 1) {
      val = *args.begin();
      args.pop_front();
    }
    if(args.size() >= 1) {
      def = *args.begin();
      args.pop_front();
    }
    ConfigurationManager::instance().add(ConfigEntry(section, variable, type, def, val));
  }
}

void
ConfigGetAllRequest::onSuccess(const QString &, const QString &)
{
  ConfigurationManager::instance().complete();
}


ConfigSaveRequest::ConfigSaveRequest(const QString &sequenceId,
				     const QString &command,
				     const std::list< QString > &args)
  : Request(sequenceId, command, args)
{}


void
ConfigSaveRequest::onError(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("ConfigSaveRequest error: (%1) %1\n")
    .arg(code)
    .arg(message);
}

void
ConfigSaveRequest::onSuccess(const QString &, const QString &)
{
  ConfigurationManager::instance().finishSave();
}

StopRequest::StopRequest(const QString &sequenceId,
			 const QString &command,
			 const std::list< QString > &args)
  : Request(sequenceId, command, args)
{}


void
StopRequest::onError(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("StopRequest error: (%1) %1\n")
    .arg(code)
    .arg(message);
}

void
StopRequest::onSuccess(const QString &, const QString &)
{
  PhoneLineManager::instance().finishStop();
}

SignalizedRequest::SignalizedRequest(const QString &sequenceId,
				     const QString &command,
				     const std::list< QString > &args)
  : Request(sequenceId, command, args)
{}

void
SignalizedRequest::onError(const QString &code, 
			   const QString &message)
{
  emit error(message, code);
}

void
SignalizedRequest::onEntry(const QString &code,
			   const QString &message)
{
  emit entry(message, code);
}

void
SignalizedRequest::onSuccess(const QString &code, 
			     const QString &message)
{
  emit success(message, code);
}

