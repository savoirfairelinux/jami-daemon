#include <iostream>

#include "globals.h"

#include "PhoneLine.hpp"
#include "PhoneLineLocker.hpp"
#include "PhoneLineManager.hpp"

PhoneLineManager::PhoneLineManager(unsigned int nbLines)
  : mAccount(mSession.getDefaultAccount())
  , mCurrentLine(NULL)
{
  for(unsigned int i = 0; i < nbLines; i++) {
    mPhoneLines.push_back(new PhoneLine());
  }
}

void PhoneLineManager::clicked()
{
  std::cout << "Clicked" << std::endl;
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
    }
  }
  else {
    _debug("Tried to selected line %d, which appears to be invalid.\n", line);
  }
}

void
PhoneLineManager::call(const QString &to)
{
}
