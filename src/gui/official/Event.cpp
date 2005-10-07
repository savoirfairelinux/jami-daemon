#include <qobject.h>

#include "globals.h"

#include "Call.hpp"
#include "DebugOutput.hpp"
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
  DebugOutput::instance() << QObject::tr("Event: Received: %1\n").arg(toString());
}

QString
Event::toString()
{
  QString output(mCode);
  for(std::list< QString >::iterator pos = mArgs.begin();
      pos != mArgs.end();
      pos++) {
    output += " ";
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
