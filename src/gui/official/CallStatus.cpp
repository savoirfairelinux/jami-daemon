#include "globals.h"

#include "CallStatus.hpp"
#include "PhoneLineManager.hpp"

CallStatus::CallStatus(const std::string &code,
		       const std::list< std::string > &args)
  : CallRelatedEvent(code, args)
{
  std::list< std::string > l = getUnusedArgs();
  if(l.size() >= 3) {
    mAccountId = *l.begin();
    l.pop_front();
    mDestination = *l.begin();
    l.pop_front();
    mStatus = *l.begin();
    l.pop_front();
    setUnusedArgs(l);
  }
}

void
CallStatus::execute()
{
  std::string id = getCallId();
  if(id.size() > 0) {
    _debug("%s status received for call ID: %s.\n", 
	   mStatus.c_str(),
	   id.c_str());
    PhoneLineManager::instance().addCall(mAccountId, getCallId(), mDestination, mStatus);
  }
  else {
    _debug("Status invalid: %s\n", toString().c_str());
  }
}

