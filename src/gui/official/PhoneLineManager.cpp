#include <iostream>

#include "PhoneLineManager.hpp"

PhoneLineManager::PhoneLineManager()
  : mAccount(mSession.getDefaultAccount())
  , mCurrentLine(NULL)
{}

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
}

void
PhoneLineManager::call(const QString &to)
{
  {
    QMutexLock currentLineGuard(&mCurrentLineMutex);
    if
}
