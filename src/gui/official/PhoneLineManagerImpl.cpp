#include <QMutexLocker>
#include <iostream>
#include <stdexcept>

#include "globals.h"

#include "Call.hpp"
#include "Event.hpp"
#include "PhoneLine.hpp"
#include "PhoneLineLocker.hpp"
#include "PhoneLineManager.hpp"

PhoneLineManagerImpl::PhoneLineManagerImpl()
  : mAccount(mSession.getDefaultAccount())
  , mCurrentLine(NULL)
{
  EventFactory::instance().registerEvent< HangupEvent >("002");
  EventFactory::instance().registerEvent< IncommingEvent >("001");
  mSession.getEvents();
}

PhoneLine *
PhoneLineManagerImpl::getCurrentLine()
{
  QMutexLocker guard(&mCurrentLineMutex);
  return mCurrentLine;
}

void 
PhoneLineManagerImpl::setNbLines(unsigned int nb)
{
  mPhoneLines.clear();
  for(unsigned int i = 0; i < nb; i++) {
    mPhoneLines.push_back(new PhoneLine(mSession, i + 1));
  }
}

PhoneLine *
PhoneLineManagerImpl::getNextAvailableLine()
{
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
  QMutexLocker guard(&mPhoneLinesMutex);
  if(mPhoneLines.size() <= line) {
    throw std::runtime_error("Trying to get an invalid Line");
  }
  
  return mPhoneLines[line];
}

PhoneLine *
PhoneLineManagerImpl::getPhoneLine(const std::string &callId)
{
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
  PhoneLine *selectedLine = getCurrentLine();

  if(!selectedLine) {
    selectedLine = selectNextAvailableLine();
  }

  if(selectedLine) {
    PhoneLineLocker guard(selectedLine);
    selectedLine->sendKey(c);
  }
}


/**
 * Warning: This function might 'cause a problem if
 * we select 2 line in a very short time.
 */
void
PhoneLineManagerImpl::selectLine(unsigned int line)
{
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
	oldLine->unselect();
      }

      PhoneLineLocker guard(selectedLine);
      selectedLine->select();
      if(selectedLine->isAvailable()) {
	mSession.playTone();
      }
    }
  }
  else {
    _debug("Tried to selected line %d, which appears to be invalid.\n", line);
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
				const std::string &,
				const std::string &callId)
{
  PhoneLine *selectedLine = getNextAvailableLine();
  PhoneLineLocker guard(selectedLine, false);

  Call call(mSession, callId, true);
  if(selectedLine) {
    selectedLine->incomming(call);
  }
  else {
    _debug("There's no available lines here for the incomming call ID: %s.\n",
	   callId.c_str());
    call.notAvailable();
  }
}
