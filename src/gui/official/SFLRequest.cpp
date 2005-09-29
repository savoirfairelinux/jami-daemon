#include <memory>
#include <sstream>
#include <string>

#include "globals.h"
#include "PhoneLineManager.hpp"
#include "SFLRequest.hpp"
#include "CallStatus.hpp"
#include "CallStatusFactory.hpp"

EventRequest::EventRequest(const std::string &sequenceId,
			   const std::string &command,
			   const std::list< std::string > &args)
  : Request(sequenceId, command, args)
{}


void
EventRequest::onError(const std::string &code, const std::string &message)
{
  _debug("EventRequest error: (%s)%s\n", code.c_str(), message.c_str());
}

void
EventRequest::onEntry(const std::string &code, const std::string &message)
{
  std::auto_ptr< Event > 
    e(EventFactory::instance().create(code, Request::parseArgs(message)));
  e->execute();
}

void
EventRequest::onSuccess(const std::string &code, const std::string &message)
{
  _debug("EventRequest success: (%s)%s\n", code.c_str(), message.c_str());
}

CallStatusRequest::CallStatusRequest(const std::string &sequenceId,
				     const std::string &command,
				     const std::list< std::string > &args)
  : Request(sequenceId, command, args)
{}


void
CallStatusRequest::onError(const std::string &code, const std::string &message)
{
  _debug("CallStatusRequest error: (%s)%s\n", code.c_str(), message.c_str());
  PhoneLineManager::instance().errorOnCallStatus();
}

void
CallStatusRequest::onEntry(const std::string &code, const std::string &message)
{
  std::auto_ptr< Event > 
    e(EventFactory::instance().create(code, Request::parseArgs(message)));
  e->execute();
}

void
CallStatusRequest::onSuccess(const std::string &code, const std::string &message)
{
  _debug("CallStatusRequest success: (%s)%s\n", code.c_str(), message.c_str());
  if(code == "206") {
    std::list< std::string > args = Request::parseArgs(message);
    if(args.size() >= 2) {
      PhoneLineManager::instance().selectLine(*args.begin(), true);
    }
    else {
      _debug("CallStatusRequest Error: cannot get current line.\n");
    }
  }
  PhoneLineManager::instance().handleEvents();
}
