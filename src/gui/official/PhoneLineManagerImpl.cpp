#include <QMutexLocker>
#include <iostream>
#include <stdexcept>

#include "globals.h"

#include "Event.hpp"
#include "PhoneLine.hpp"
#include "PhoneLineLocker.hpp"
#include "PhoneLineManager.hpp"

PhoneLineManagerImpl::PhoneLineManagerImpl()
  : mAccount(mSession.getDefaultAccount())
  , mCurrentLine(NULL)
{
  EventFactory::instance().registerEvent< HangupEvent >("002");
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
PhoneLineManagerImpl::selectNextAvailableLine()
{
  PhoneLine *selectedLine = NULL;

  QMutexLocker guard(&mPhoneLinesMutex);
  QMutexLocker guard2(&mCurrentLineMutex);

  unsigned int i = 0;
  while(i < mPhoneLines.size() && !selectedLine) {
    PhoneLineLocker guard(mPhoneLines[i]);
    if(mPhoneLines[i]->isAvailable() && 
       mPhoneLines[i] != mCurrentLine) {
      selectedLine = mPhoneLines[i];
    }
    else {
      i++;
    }
  }

  // If we found one available line.
  if(selectedLine) {
    if(mCurrentLine) {
      PhoneLineLocker guard(mCurrentLine);
      mCurrentLine->unselect();
    }
    mCurrentLine = selectedLine;
    
    // select current line.
    {
      PhoneLineLocker guard(mCurrentLine);
      mCurrentLine->select();
    }
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

