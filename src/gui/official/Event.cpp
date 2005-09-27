#include "globals.h"

#include "Call.hpp"
#include "Event.hpp"
#include "PhoneLineManager.hpp"

Event::Event(const std::string &code,
	     const std::list< std::string > &args)
  : mCode(code)
  , mUnusedArgs(args)
  , mArgs(args)
{}

std::string
Event::toString()
{
  std::string output(mCode);
  for(std::list< std::string >::iterator pos = mArgs.begin();
      pos != mArgs.end();
      pos++) {
    output += *pos;
  }
  
  return output;
}

CallRelatedEvent::CallRelatedEvent(const std::string &code,
				   const std::list< std::string > &args)
  : Event(code, args)
{
  std::list< std::string > l(getUnusedArgs());
  if(l.size() != 0) {
    mCallId = *l.begin();
    l.pop_front();
    setUnusedArgs(l);
  }
}

std::string
CallRelatedEvent::getCallId()
{
  return mCallId;
}

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
  std::list< std::string > l;
  if(getUnusedArgs().size() >= 3) {
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
    _debug("Hangup Event received for call ID: %s.\n", id.c_str());
    PhoneLineManager::instance().incomming(mAccountId, mOrigin, getCallId());
  }
  else {
    _debug("Event invalid: %s\n", toString().c_str());
  }
}
