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
  if(id.length() > 0) {
    DebugOutput::instance() << QObject::tr("%1 status received for call ID: %2.\n")
      .arg(mStatus)
      .arg(id);
    PhoneLineManager::instance().addCall(mAccountId, getCallId(), mDestination, mStatus);
  }
  else {
    DebugOutput::instance() << QObject::tr("Status invalid: %1\n").arg(toString());
  }
}

