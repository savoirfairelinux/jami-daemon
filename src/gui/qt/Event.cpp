/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *                                                                              
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                              
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

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
