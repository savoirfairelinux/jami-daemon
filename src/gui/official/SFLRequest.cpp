#include <iostream>
#include <memory>
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
  PhoneLineManager::instance().start();
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
    line->setLineStatus(message);
    line->setAction("");
    line->error();
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
      line->setLineStatus(message);
      line->error();
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
  if(args.size() >= 4) {
    QString section, variable, type, def, val;
    section = *args.begin();
    args.pop_front();
    variable = *args.begin();
    args.pop_front();
    type = *args.begin();
    args.pop_front();
    val = *args.begin();
    args.pop_front();
    if(args.size() >= 1) {
      def = *args.begin();
    }
    ConfigurationManager::instance().add(ConfigEntry(section, variable, type, def, val));
  }
}

void
ConfigGetAllRequest::onSuccess(const QString &, const QString &)
{
  ConfigurationManager::instance().complete();
}


ListRequest::ListRequest(const QString &sequenceId,
			 const QString &command,
			 const std::list< QString > &args)
  : Request(sequenceId, command, args)
{}


void
ListRequest::onError(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("ListRequest error: (%1) %1\n")
    .arg(code)
    .arg(message);
}

void
ListRequest::onEntry(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("ListRequest entry: (%1) %1\n")
    .arg(code)
    .arg(message);
  std::list< QString > args = Request::parseArgs(message);
  if(args.size() >= 2) {
    AudioDevice device;
    device.index = *args.begin();
    args.pop_front();
    device.description = *args.begin();
    args.pop_front();
    ConfigurationManager::instance().add(device);
  }
}

void
ListRequest::onSuccess(const QString &code, const QString &message)
{
  DebugOutput::instance() << QObject::tr("ListRequest success: (%1) %1\n")
    .arg(code)
    .arg(message);
}
