#include <QMutexLocker>
#include <iostream>
#include <stdexcept>

#include "globals.h"

#include "PhoneLine.hpp"
#include "PhoneLineLocker.hpp"
#include "PhoneLineManager.hpp"

PhoneLineManager::PhoneLineManager(unsigned int nbLines)
  : mAccount(mSession.getDefaultAccount())
  , mCurrentLine(NULL)
{
  for(unsigned int i = 0; i < nbLines; i++) {
    mPhoneLines.push_back(new PhoneLine(mSession.createCall(),
					i));
  }
}

void 
PhoneLineManager::selectAvailableLine()
{
  PhoneLine *selectedLine = NULL;

  mPhoneLinesMutex.lock();
  unsigned int i = 0;
  while(i < mPhoneLines.size() && !selectedLine) {
    PhoneLineLocker guard(mPhoneLines[i]);
    if(mPhoneLines[i]->isAvailable()) {
      selectedLine = mPhoneLines[i];
    }
    else {
      i++;
    }
  }
  mPhoneLinesMutex.unlock();
  
  if(selectedLine) {
    selectLine(i);
  }
}

PhoneLine *
PhoneLineManager::getPhoneLine(unsigned int line)
{
  QMutexLocker guard(&mPhoneLinesMutex);
  if(mPhoneLines.size() <= line) {
    throw std::runtime_error("Trying to get an invalid Line");
  }
  
  return mPhoneLines[line];
}

void
PhoneLineManager::sendKey(Qt::Key c)
{
  PhoneLine *selectedLine = NULL;
  mCurrentLineMutex.lock();
  selectedLine = mCurrentLine;
  mCurrentLineMutex.unlock();

  if(!selectedLine) {
    selectAvailableLine();
    mCurrentLineMutex.lock();
    selectedLine = mCurrentLine;
    mCurrentLineMutex.unlock();
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
PhoneLineManager::selectLine(unsigned int line)
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
    _debug("Line %d selected.\n", line);
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
	mSession.sendTone();
      }
    }
  }
  else {
    _debug("Tried to selected line %d, which appears to be invalid.\n", line);
  }
}

void
PhoneLineManager::call(const QString &)
{
}
