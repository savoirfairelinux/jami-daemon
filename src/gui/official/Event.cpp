#include "globals.h"

#include "Call.hpp"
#include "Event.hpp"

Event::Event(const std::string &code,
	     const std::list< std::string > &args)
  : mCode(code)
  , mUnusedArgs(args)
  , mArgs(args)
{}


void
Event::execute()
{
  _debug("Received: %s\n", toString().c_str());
}

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
