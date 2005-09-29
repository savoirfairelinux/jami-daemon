#include <QMutexLocker>
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
{
  EventFactory::instance().registerEvent< IncommingEvent >("001");
  EventFactory::instance().registerEvent< HangupEvent >("002");
  EventFactory::instance().registerEvent< TryingStatus >("110");
  EventFactory::instance().registerEvent< RingingStatus >("111");
  EventFactory::instance().registerEvent< HoldStatus >("112");
  EventFactory::instance().registerEvent< EstablishedStatus >("113");
  EventFactory::instance().registerEvent< BusyStatus >("114");
  EventFactory::instance().registerEvent< CongestionStatus >("115");
  EventFactory::instance().registerEvent< WrongNumberStatus >("116");
  QObject::connect(this, SIGNAL(disconnected()),
		   this, SLOT(closeSession()));
  QObject::connect(this, SIGNAL(readyToHandleEvents),
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
  mSession->connect();
}

void 
PhoneLineManagerImpl::startSession()
{
  isInitialized();

  mSession->getCallStatus();
}

void 
PhoneLineManagerImpl::handleEvents()
{
  isInitialized();

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
    mPhoneLines.push_back(new PhoneLine(*mSession, i + 1));
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
PhoneLineManagerImpl::getPhoneLine(const std::string &callId)
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
PhoneLineManagerImpl::selectLine(const std::string &callId, bool hardselect)
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
    _debug("PhoneLineManager: Tried to selected line with call ID (%s), "
	   "which appears to be invalid.\n", callId.c_str());
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
      if(selectedLine->isAvailable()) {
	mSession->playTone();
      }
    }
  }
  else {
    _debug("PhoneLineManager: Tried to selected line %d, which appears to be invalid.\n", line);
  }
}

void
PhoneLineManagerImpl::call(const QString &to)
{
  PhoneLine *current = getCurrentLine();
  if(current) {
    PhoneLineLocker guard(current);
    current->call(to.toStdString());
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
  }
}

void
PhoneLineManagerImpl::hangup(const std::string &callId)
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
PhoneLineManagerImpl::incomming(const std::string &,
				const std::string &peer,
				const std::string &callId)
{
  Call call(*mSession, callId, true);
  addCall(call, peer, "Incomming");
}

void 
PhoneLineManagerImpl::addCall(const std::string &,
			      const std::string &callId,
			      const std::string &peer,
			      const std::string &state)
{
  addCall(Call(*mSession, callId), peer, state);
}

void 
PhoneLineManagerImpl::addCall(Call call,
			      const std::string &peer,
			      const std::string &state)
{
  PhoneLine *selectedLine = getNextAvailableLine();
  PhoneLineLocker guard(selectedLine, false);

  if(selectedLine) {
    selectedLine->incomming(call);
    selectedLine->setPeer(peer);
    selectedLine->setState(state);
  }
  else {
    _debug("PhoneLineManager: There's no available lines here for the incomming call ID: %s.\n",
	   call.id().c_str());
    call.notAvailable();
  }
}
