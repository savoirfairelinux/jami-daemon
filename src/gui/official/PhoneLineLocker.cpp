#include "PhoneLineLocker.hpp"
#include "PhoneLine.hpp"

PhoneLineLocker::PhoneLineLocker(PhoneLine *line)
  : mPhoneLine(line)
{
  if(mPhoneLine) {
    mPhoneLine->lock();
  }
}

PhoneLineLocker::~PhoneLineLocker()
{
  if(mPhoneLine) {
    mPhoneLine->unlock();
  }
}


