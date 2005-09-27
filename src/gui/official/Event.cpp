#include "globals.h"
#include "Event.hpp"
#include "PhoneLineManager.hpp"

Event::Event(const std::string &code,
	     const std::list< std::string > &args)
  : mCode(code)
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

HangupEvent::HangupEvent(const std::string &code,
			 const std::list< std::string > &args)
  : Event(code, args)
{
  if(args.size() != 0) {
    mCallId = *args.begin();
  }
}

void
HangupEvent::execute()
{
  if(mCallId.size() > 0) {
    _debug("Hangup Event received for call ID: %s.\n", mCallId.c_str());
    PhoneLineManager::instance().hangup(mCallId);
  }
  else {
    _debug("Event invalid: %s\n", toString().c_str());
  }
}
