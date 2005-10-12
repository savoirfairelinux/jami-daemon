#include <qmutex.h>
#include <iostream>
#include <stdexcept>

#include "globals.h"

#include "CallStatusFactory.hpp"
#include "SFLEvents.hpp"
#include "SFLCallStatus.hpp"
#include "PhoneLine.hpp"
#include "PhoneLineLocker.hpp"
#include "PhoneLineManager.hpp"

PhoneLineManagerImpl::PhoneLineManagerImpl()
  : mSession(NULL)
  , mAccount(NULL)
  , mCurrentLine(NULL)
  , mIsInitialized(false)
  , mVolume(-1)
  , mMicVolume(-1)
{
  EventFactory::instance().registerDefaultEvent< DefaultEvent >();
  // TODO: 000
  EventFactory::instance().registerEvent< CallRelatedEvent >("000");
  EventFactory::instance().registerEvent< IncommingEvent >("001");
  EventFactory::instance().registerEvent< HangupEvent >("002");
  // TODO: 020
  EventFactory::instance().registerEvent< CallRelatedEvent >("020");
  EventFactory::instance().registerEvent< VolumeEvent >("021");
  EventFactory::instance().registerEvent< MicVolumeEvent >("022");
  EventFactory::instance().registerEvent< TryingStatus >("110");
  EventFactory::instance().registerEvent< RingingStatus >("111");
  EventFactory::instance().registerEvent< HoldStatus >("112");
  EventFactory::instance().registerEvent< EstablishedStatus >("113");
  EventFactory::instance().registerEvent< BusyStatus >("114");
  EventFactory::instance().registerEvent< CongestionStatus >("115");
  EventFactory::instance().registerEvent< WrongNumberStatus >("116");
  QObject::connect(this, SIGNAL(disconnected()),
		   this, SLOT(closeSession()));
  QObject::connect(this, SIGNAL(readyToHandleEvents()),
		   this, SLOT(handleEvents()));
  QObject::connect(this, SIGNAL(connected()),
		   this, SIGNAL(readyToSendStatus()));
  QObject::connect(this, SIGNAL(readyToSendStatus()),
		   this, SLOT(startSession()));
  
}

PhoneLineManagerImpl::~PhoneLineManagerImpl()
{
  delete mSession;
  delete mAccount;
  for(std::vector< PhoneLine * >::iterator pos = mPhoneLines.begin();
      pos != mPhoneLines.end();
      pos++) {
    delete *pos;
  }
}

void
PhoneLineManagerImpl::initialize()
{
  QMutexLocker guard(&mIsInitializedMutex);
  if(!mIsInitialized) {
    mIsInitialized = true;
    mSession = new Session();
    mAccount = new Account(mSession->getDefaultAccount());
  }
}

void 
PhoneLineManagerImpl::start()
{
  isInitialized();

  emit globalStatusSet(QString(tr("Trying to connect to sflphone server..")));
  mSession->connect();
}

void PhoneLineManagerImpl::isInitialized()
{
  QMutexLocker guard(&mIsInitializedMutex);
  if(!mIsInitialized) {
    throw std::logic_error("Trying to use PhoneLineManager without prior initialize.");
  }
}

void
PhoneLineManagerImpl::connect()
{
  isInitialized();

  emit globalStatusSet(QString(tr("Trying to connect to sflphone server..")));
  mSession->connect();
}

void 
PhoneLineManagerImpl::startSession()
{
  isInitialized();

  closeSession();
  emit globalStatusSet(QString(tr("Trying to get line status...")));
  mSession->getCallStatus();
}

void 
PhoneLineManagerImpl::handleEvents()
{
  isInitialized();

  emit globalStatusSet(QString(tr("SFLPhone is ready to serve you, master.")));
  mSession->getEvents();
}


void 
PhoneLineManagerImpl::closeSession()
{
  isInitialized();

  QMutexLocker guard(&mPhoneLinesMutex);
  mCurrentLineMutex.lock();
  mCurrentLine = NULL;
  mCurrentLineMutex.unlock();
    
  unsigned int i = 0;
  while(i < mPhoneLines.size()) {
    PhoneLineLocker guard(mPhoneLines[i]);
    mPhoneLines[i]->disconnect();
    i++;
  }

  emit lineStatusSet("");
  emit bufferStatusSet("");
  emit globalStatusSet("Disconnected.");
}


PhoneLine *
PhoneLineManagerImpl::getCurrentLine()
{
  isInitialized();

  QMutexLocker guard(&mCurrentLineMutex);
  return mCurrentLine;
}

void 
PhoneLineManagerImpl::setNbLines(unsigned int nb)
{
  isInitialized();

  QMutexLocker guard(&mPhoneLinesMutex);
  mPhoneLines.clear();
  for(unsigned int i = 0; i < nb; i++) {
    PhoneLine *p = new PhoneLine(*mSession, *mAccount, i + 1);
    QObject::connect(p, SIGNAL(lineStatusChanged(QString)),
		     this, SIGNAL(lineStatusSet(QString)));
    QObject::connect(p, SIGNAL(actionChanged(QString)),
		     this, SIGNAL(actionSet(QString)));
    QObject::connect(p, SIGNAL(bufferStatusChanged(QString)),
		     this, SIGNAL(bufferStatusSet(QString)));
    mPhoneLines.push_back(p);
  }
}

PhoneLine *
PhoneLineManagerImpl::getNextAvailableLine()
{
  isInitialized();

  PhoneLine *selectedLine = NULL;

  QMutexLocker guard(&mPhoneLinesMutex);
  mCurrentLineMutex.lock();
  PhoneLine *current = mCurrentLine;
  mCurrentLineMutex.unlock();
    

  unsigned int i = 0;
  while(i < mPhoneLines.size() && !selectedLine) {
    mPhoneLines[i]->lock();
    if(mPhoneLines[i]->isAvailable() && 
       mPhoneLines[i] != current) {
      selectedLine = mPhoneLines[i];
    }
    else {
      mPhoneLines[i]->unlock();
      i++;
    }
  }

  return selectedLine;
}

PhoneLine *
PhoneLineManagerImpl::getLine(const Call &call)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;
  QMutexLocker guard(&mPhoneLinesMutex);

  unsigned int i = 0;
  while(i < mPhoneLines.size() && !selectedLine) {
    mPhoneLines[i]->lock();
    if(mPhoneLines[i]->getCallId() == call.id()) {
      selectedLine = mPhoneLines[i];
    }
    else {
      mPhoneLines[i]->unlock();
      i++;
    }
  }

  return selectedLine;
}

PhoneLine *
PhoneLineManagerImpl::getLine(unsigned int line)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;

  QMutexLocker guard(&mPhoneLinesMutex);
  if(line < mPhoneLines.size()) {
    selectedLine = mPhoneLines[line];
    selectedLine->lock();
  }

  return selectedLine;
}

PhoneLine *
PhoneLineManagerImpl::selectNextAvailableLine()
{
  isInitialized();

  PhoneLine *selectedLine = getNextAvailableLine();
  PhoneLineLocker guard(selectedLine, false);

  // If we found one available line.
  if(selectedLine) {
    QMutexLocker guard(&mCurrentLineMutex);
    if(mCurrentLine) {
      PhoneLineLocker guard(mCurrentLine);
      mCurrentLine->unselect();
    }
    mCurrentLine = selectedLine;
    
    // select current line.
    // We don't need to lock it, since it is
    // done at the top.
    selectedLine->select();
    emit lineStatusSet(selectedLine->getLineStatus());
    emit bufferStatusSet(selectedLine->getBuffer());
  }

  return selectedLine;
}



PhoneLine *
PhoneLineManagerImpl::getPhoneLine(unsigned int line)
{
  isInitialized();

  QMutexLocker guard(&mPhoneLinesMutex);
  if(mPhoneLines.size() <= line) {
    throw std::runtime_error("Trying to get an invalid Line");
  }
  
  return mPhoneLines[line];
}

PhoneLine *
PhoneLineManagerImpl::getPhoneLine(const QString &callId)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;

  QMutexLocker guard(&mPhoneLinesMutex);
  unsigned int i = 0;
  while(i < mPhoneLines.size() &&
	!selectedLine) {
    if(mPhoneLines[i]->getCallId() == callId) {
      selectedLine = mPhoneLines[i];
    }
    else {
      i++;
    }
  }
  
  return selectedLine;
}


void
PhoneLineManagerImpl::sendKey(Qt::Key c)
{
  isInitialized();

  PhoneLine *selectedLine = getCurrentLine();

  // Only digits that select a line if there's
  // no current line.
  if (!selectedLine && QChar(c).isDigit()) {
    selectedLine = selectNextAvailableLine();
  }
  
  if(selectedLine) {
    PhoneLineLocker guard(selectedLine);
    selectedLine->sendKey(c);
  }
}


void 
PhoneLineManagerImpl::selectLine(const QString &callId, bool hardselect)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;
  mPhoneLinesMutex.lock();
  unsigned int line = 0;
  while(!selectedLine && line < mPhoneLines.size()) {
    if(mPhoneLines[line]->getCallId() == callId) {
      selectedLine = mPhoneLines[line];
    }
    else {
      line++;
    }
  }
  mPhoneLinesMutex.unlock();

  if(selectedLine) {
    selectLine(line, hardselect);
  }
  else {
    DebugOutput::instance() << QObject::tr("PhoneLineManager: Tried to selected line with call ID (%1), "
					   "which appears to be invalid.\n").arg(callId);
  }
}

/**
 * Warning: This function might 'cause a problem if
 * we select 2 line in a very short time.
 */
void
PhoneLineManagerImpl::selectLine(unsigned int line, bool hardselect)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;
  // getting the wanted line;
  {
    mPhoneLinesMutex.lock();
    if(mPhoneLines.size() > line) {
      selectedLine = mPhoneLines[line];
    }
    mPhoneLinesMutex.unlock();
  }
   
  if(selectedLine != NULL) {
    mCurrentLineMutex.lock();
    PhoneLine *oldLine = mCurrentLine;
    mCurrentLine = selectedLine;
    mCurrentLineMutex.unlock();
    
    if(oldLine != selectedLine) {
      if(oldLine != NULL) {
	PhoneLineLocker guard(oldLine);
	oldLine->unselect(hardselect);
      }

      PhoneLineLocker guard(selectedLine);
      selectedLine->select(hardselect);
      emit lineStatusSet(selectedLine->getLineStatus());
      emit bufferStatusSet(selectedLine->getBuffer());
      if(selectedLine->isAvailable()) {
	mSession->playTone();
      }
    }
  }
  else {
    DebugOutput::instance() << QObject::tr("PhoneLineManager: Tried to selected line %1, "
					   "which appears to be invalid.\n").arg(line);
  }
}

void
PhoneLineManagerImpl::call(const QString &to)
{
  PhoneLine *current = getCurrentLine();
  if(current) {
    PhoneLineLocker guard(current);
    current->call(to);
  }
}


void
PhoneLineManagerImpl::call()
{
  PhoneLine *current = getCurrentLine();
  if(current) {
    PhoneLineLocker guard(current);
    current->call();
  }
}

void
PhoneLineManagerImpl::hold()
{
  mCurrentLineMutex.lock();
  PhoneLine *selectedLine = mCurrentLine;
  PhoneLineLocker guard(selectedLine);
  mCurrentLine = NULL;
  mCurrentLineMutex.unlock();

  if(selectedLine) {
    selectedLine->hold();
  }
}

void
PhoneLineManagerImpl::hangup()
{
  mCurrentLineMutex.lock();
  PhoneLine *selectedLine = mCurrentLine;
  PhoneLineLocker guard(selectedLine);
  mCurrentLine = NULL;
  mCurrentLineMutex.unlock();

  if(selectedLine) {
    selectedLine->hangup();
    lineStatusSet("");
  }
}

void
PhoneLineManagerImpl::mute(bool muting)
{
  if(muting) {
    mute();
  }
  else {
    unmute();
  }
}

void
PhoneLineManagerImpl::mute()
{
  isInitialized();
  
  mSession->mute();
}

void
PhoneLineManagerImpl::unmute()
{
  isInitialized();

  mSession->unmute();
}

void
PhoneLineManagerImpl::hangup(const QString &callId)
{
  PhoneLine *selectedLine = getPhoneLine(callId);
  if(selectedLine) {
    PhoneLineLocker guard(selectedLine);
    selectedLine->hangup();
  }
}

void
PhoneLineManagerImpl::hangup(unsigned int line)
{
  PhoneLine *selectedLine = getPhoneLine(line);
  if(selectedLine) {
    PhoneLineLocker guard(selectedLine);
    selectedLine->hangup();
  }
}

void
PhoneLineManagerImpl::clear()
{
  mCurrentLineMutex.lock();
  PhoneLine *selectedLine = mCurrentLine;
  PhoneLineLocker guard(selectedLine);
  mCurrentLineMutex.unlock();

  if(selectedLine) {
    selectedLine->clear();
  }
}

void 
PhoneLineManagerImpl::incomming(const QString &accountId,
				const QString &peer,
				const QString &callId)
{
  Call call(mSession->id(), accountId, callId, true);
  addCall(call, peer, "Incomming");
}

void 
PhoneLineManagerImpl::addCall(const QString &accountId,
			      const QString &callId,
			      const QString &peer,
			      const QString &state)
{
  addCall(Call(mSession->id(), accountId, callId), peer, state);
}

void 
PhoneLineManagerImpl::addCall(Call call,
			      const QString &peer,
			      const QString &state)
{
  PhoneLine *selectedLine = getNextAvailableLine();
  PhoneLineLocker guard(selectedLine, false);

  if(selectedLine) {
    selectedLine->incomming(call);
    selectedLine->setPeer(peer);
    selectedLine->setState(state);
  }
  else {
    DebugOutput::instance() << QObject::tr("PhoneLineManager: There's no available lines"
					     "here for the incomming call ID: %1.\n")
      .arg(call.id());
    call.notAvailable();
  }
}

void
PhoneLineManagerImpl::updateVolume(int volume)
{
  mVolume = volume;
  emit volumeUpdated((unsigned int)volume);
}

void
PhoneLineManagerImpl::updateMicVolume(int volume)
{
  mMicVolume = volume;
  emit micVolumeUpdated((unsigned int)volume);
}

void 
PhoneLineManagerImpl::setVolume(int volume)
{
  if(mVolume != volume) {
    mSession->volume(volume);
    updateVolume(volume);
  }
}

void 
PhoneLineManagerImpl::setMicVolume(int volume)
{
  if(mMicVolume != volume) {
    mSession->micVolume(volume);
    updateMicVolume(volume);
  }
}
