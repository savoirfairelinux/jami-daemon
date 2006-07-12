/*
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

