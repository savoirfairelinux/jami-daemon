#include "globals.h"

#include "PhoneLineManager.hpp"
#include "SFLEvents.hpp"

DefaultEvent::DefaultEvent(const QString &code,
			 const std::list< QString > &args)
  : Event(code, args)
{
}

void
DefaultEvent::execute()
{
  _debug("DefaultEvent: We don't handle: %s\n", toString().toStdString().c_str());
}


HangupEvent::HangupEvent(const QString &code,
			 const std::list< QString > &args)
  : CallRelatedEvent(code, args)
{}

void
HangupEvent::execute()
{
  QString id = getCallId();
  if(id.size() > 0) {
    _debug("Hangup Event received for call ID: %s.\n", id.toStdString().c_str());
    PhoneLineManager::instance().hangup(id);
  }
  else {
    _debug("Hangup Event invalid (missing call ID): %s\n", toString().toStdString().c_str());
  }
}

IncommingEvent::IncommingEvent(const QString &code,
			       const std::list< QString > &args)
  : CallRelatedEvent(code, args)
{
  std::list< QString > l = getUnusedArgs();
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
  QString id = getCallId();
  if(id.size() > 0) {
    _debug("Incomming Event received for call ID: %s.\n", id.toStdString().c_str());
    PhoneLineManager::instance().incomming(mAccountId, getCallId(), mOrigin);
  }
  else {
    _debug("Incomming Event invalid: %s\n", toString().toStdString().c_str());
  }
}
