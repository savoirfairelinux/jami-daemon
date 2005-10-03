#include <memory>
#include <sstream>
#include <string>
#include <QString>

#include "globals.h"
#include "CallStatus.hpp"
#include "CallStatusFactory.hpp"
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
  _debug("EventRequest error: (%s)%s\n", 
	 code.toStdString().c_str(), 
	 message.toStdString().c_str());
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
  _debug("EventRequest success: (%s)%s\n", 
	 code.toStdString().c_str(), 
	 message.toStdString().c_str());
}

CallStatusRequest::CallStatusRequest(const QString &sequenceId,
				     const QString &command,
				     const std::list< QString > &args)
  : Request(sequenceId, command, args)
{}


void
CallStatusRequest::onError(const QString &code, const QString &message)
{
  _debug("CallStatusRequest error: (%s)%s\n", 
	 code.toStdString().c_str(), 
	 message.toStdString().c_str());
  PhoneLineManager::instance().errorOnCallStatus();
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
  _debug("CallStatusRequest success: (%s)%s\n", 
	 code.toStdString().c_str(), 
	 message.toStdString().c_str());
  if(code == "206") {
    std::list< QString > args = Request::parseArgs(message);
    if(args.size() >= 2) {
      PhoneLineManager::instance().selectLine(*args.begin(), true);
    }
    else {
      _debug("CallStatusRequest Error: cannot get current line.\n");
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
  PhoneLine *line = PhoneLineManager::instance().getLine(call);
  if(line) {
    PhoneLineLocker guard(line, false);
    line->setLineStatus(message);
    line->error();
  }
  else {
    _debug("We received an error on a call "
	   "that doesn't have a phone line (%s).\n", 
	   call.id().toStdString().c_str());
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
  }
  else {
    _debug("We received a status on a call related request "
	   "that doesn't have a phone line (%s).\n", 
	   call.id().toStdString().c_str());
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
  }
  else {
    _debug("We received a success on a call related request "
	   "that doesn't have a phone line (%s).\n", 
	   call.id().toStdString().c_str());
  }
}

TemporaryRequest::TemporaryRequest(const QString &sequenceId,
				   const QString &command,
				   const std::list< QString > &args)
  : CallRelatedRequest(sequenceId, command, args)
{}

void
TemporaryRequest::onError(Call call, 
			  const QString &, 
			  const QString &message)
{
  PhoneLine *line = PhoneLineManager::instance().getLine(call);
  if(line) {
    PhoneLineLocker guard(line, false);
    line->setAction(message);
  }
  else {
    _debug("We received an error on a temporary call "
	   "related request that doesn't have a phone "
	   "line (%s).\n", 
	   call.id().toStdString().c_str());
  }
}

void
TemporaryRequest::onEntry(Call call, 
			  const QString &, 
			  const QString &message)
{
  PhoneLine *line = PhoneLineManager::instance().getLine(call);
  if(line) {
    PhoneLineLocker guard(line, false);
    line->setAction(message);
  }
  else {
    _debug("We received a status on a temporary call "
	   "related request that doesn't have a phone "
	   "line (%s).\n", 
	   call.id().toStdString().c_str());
  }
}

void
TemporaryRequest::onSuccess(Call call, 
		       const QString &, 
			    const QString &message)
{
  PhoneLine *line = PhoneLineManager::instance().getLine(call);
  if(line) {
    PhoneLineLocker guard(line, false);
    line->setAction(message);
  }
  else {
    _debug("We received a success on a temporary call "
	   "related request that doesn't have a phone "
	   "line (%s).\n", 
	   call.id().toStdString().c_str());
  }
}
