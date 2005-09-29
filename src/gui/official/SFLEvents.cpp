#include "globals.h"

#include "PhoneLineManager.hpp"
#include "SFLEvents.hpp"

HangupEvent::HangupEvent(const std::string &code,
			 const std::list< std::string > &args)
  : CallRelatedEvent(code, args)
{}

void
HangupEvent::execute()
{
  std::string id = getCallId();
  if(id.size() > 0) {
    _debug("Hangup Event received for call ID: %s.\n", id.c_str());
    PhoneLineManager::instance().hangup(id);
  }
  else {
    _debug("Hangup Event invalid (missing call ID): %s\n", toString().c_str());
  }
}

IncommingEvent::IncommingEvent(const std::string &code,
			       const std::list< std::string > &args)
  : CallRelatedEvent(code, args)
{
  std::list< std::string > l = getUnusedArgs();
  if(l.size() >= 3) {
    mAccountId = *l.begin();
    l.pop_front();
    mOrigin = *l.begin();
    l.pop_front();
    setUnusedArgs(l);
  }
}

void
IncommingEvent::execute()
{
  std::string id = getCallId();
  if(id.size() > 0) {
    _debug("Incomming Event received for call ID: %s.\n", id.c_str());
    PhoneLineManager::instance().incomming(mAccountId, getCallId(), mOrigin);
  }
  else {
    _debug("Incomming Event invalid: %s\n", toString().c_str());
  }
}
