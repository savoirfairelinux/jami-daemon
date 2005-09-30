#include "globals.h"

#include "CallStatus.hpp"
#include "PhoneLineManager.hpp"

CallStatus::CallStatus(const QString &code,
		       const std::list< QString > &args)
  : CallRelatedEvent(code, args)
{
  std::list< QString > l = getUnusedArgs();
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
  QString id = getCallId();
  if(id.size() > 0) {
    _debug("%s status received for call ID: %s.\n", 
	   mStatus.toStdString().c_str(),
	   id.toStdString().c_str());
    PhoneLineManager::instance().addCall(mAccountId, getCallId(), mDestination, mStatus);
  }
  else {
    _debug("Status invalid: %s\n", toString().toStdString().c_str());
  }
}

