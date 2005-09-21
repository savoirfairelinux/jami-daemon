
#include "PhoneLineManager.hpp"

PhoneLineManager::PhoneLineManager()
  : mAccount(mSession.getDefaultAccount())
  , mCurrentLine(NULL)
{}

void
PhoneLineManager::selectLine(int line)
{
  PhoneLine *newline = NULL;
  QMutexLock currentLineGuard(&mCurrentLineMutex);
  {
    QMutexLock phoneLinesGuard(&mPhoneLinesMutex);
    if(mPhoneLines.size() > line) {
      if(mCurrentLine != mPhoneLines[line]) {
	if(mCurrentLine != NULL) {
	  mCurrentLine->unselect();
	}
	mCurrentLine = mPhoneLines[line];
	mCurrentLine->select();
      }
    }
  }
    
}
