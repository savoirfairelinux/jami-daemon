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
  DebugOutput::instance() << QObject::tr("DefaultEvent: We don't handle: %1\n").arg(toString());
}


HangupEvent::HangupEvent(const QString &code,
			 const std::list< QString > &args)
  : CallRelatedEvent(code, args)
{}

void
HangupEvent::execute()
{
  QString id = getCallId();
  if(id.length() > 0) {
    DebugOutput::instance() << QObject::tr("Hangup Event received for call ID: %1.\n")
      .arg(id);
    PhoneLineManager::instance().hangup(id, false);
  }
  else {
    DebugOutput::instance() << QObject::tr("Hangup Event invalid (missing call ID): %1\n")
      .arg(toString());
  }
}

IncommingEvent::IncommingEvent(const QString &code,
			       const std::list< QString > &args)
  : CallRelatedEvent(code, args)
{
  std::list< QString > l = getUnusedArgs();
  if(l.size() >= 2) {
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
  if(id.length() > 0) {
    DebugOutput::instance() << QObject::tr("Incomming Event received for call ID: %1.\n")
      .arg(id);
    PhoneLineManager::instance().incomming(mAccountId, getCallId(), mOrigin);
  }
  else {
    DebugOutput::instance() << QObject::tr("Incomming Event invalid: %1\n")
      .arg(toString());
  }
}

VolumeEvent::VolumeEvent(const QString &code,
			 const std::list< QString > &args)
  : Event(code, args)
{
  std::list< QString > l = getUnusedArgs();
  if(l.size() >= 1) {
    mVolume = l.begin()->toUInt();
    l.pop_front();
    setUnusedArgs(l);
  }
}

void
VolumeEvent::execute()
{
  PhoneLineManager::instance().updateVolume(mVolume);
}

MicVolumeEvent::MicVolumeEvent(const QString &code,
			       const std::list< QString > &args)
  : VolumeEvent(code, args)
{}

void
MicVolumeEvent::execute()
{
  PhoneLineManager::instance().updateMicVolume(mVolume);
}

MessageTextEvent::MessageTextEvent(const QString &code,
			 const std::list< QString > &args)
  : Event(code, args)
{
  std::list< QString > l = getUnusedArgs();
  if(l.size() >= 1) {
    mMessage = *l.begin();
    l.pop_front();
    setUnusedArgs(l);
  }
}

void
MessageTextEvent::execute()
{
  PhoneLineManager::instance().incomingMessageText(mMessage);
}

LoadSetupEvent::LoadSetupEvent(const QString &code,
			       const std::list< QString > &args)
  : Event(code, args)
{}

void
LoadSetupEvent::execute()
{
  PhoneLineManager::instance().setup();
}
