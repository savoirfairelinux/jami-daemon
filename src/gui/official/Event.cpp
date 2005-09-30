#include "globals.h"

#include "Call.hpp"
#include "Event.hpp"

Event::Event(const QString &code,
	     const std::list< QString > &args)
  : mCode(code)
  , mUnusedArgs(args)
  , mArgs(args)
{}


void
Event::execute()
{
  _debug("Received: %s\n", toString().toStdString().c_str());
}

QString
Event::toString()
{
  QString output(mCode);
  for(std::list< QString >::iterator pos = mArgs.begin();
      pos != mArgs.end();
      pos++) {
    output += *pos;
  }
  
  return output;
}

CallRelatedEvent::CallRelatedEvent(const QString &code,
				   const std::list< QString > &args)
  : Event(code, args)
{
  std::list< QString > l(getUnusedArgs());
  if(l.size() != 0) {
    mCallId = *l.begin();
    l.pop_front();
    setUnusedArgs(l);
  }
}

QString
CallRelatedEvent::getCallId()
{
  return mCallId;
}
