#include <memory>
#include <sstream>
#include <string>

#include "globals.h"
#include "PhoneLineManager.hpp"
#include "SFLRequest.hpp"

EventRequest::EventRequest(const std::string &sequenceId,
			   const std::string &command,
			   const std::list< std::string > &args)
  : Request(sequenceId, command, args)
{}


void
EventRequest::onError(const std::string &code, const std::string &message)
{
  _debug("EventRequest error: (%s) %s", code.c_str(), message.c_str());
}

void
EventRequest::onEntry(const std::string &code, const std::string &message)
{
  std::istringstream stream(message);
  std::string s;
  std::list< std::string > args;
  while(stream.good()) {
    stream >> s;
    args.push_back(s);
  }
  std::auto_ptr< Event > e(EventFactory::instance().create(code, args));
  e->execute();
}

void
EventRequest::onSuccess(const std::string &code, const std::string &message)
{
  _debug("EventRequest success: (%s) %s", code.c_str(), message.c_str());
}
