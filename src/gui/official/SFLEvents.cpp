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
    PhoneLineManager::instance().hangup(id);
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
