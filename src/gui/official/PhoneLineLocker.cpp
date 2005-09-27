#include "PhoneLineLocker.hpp"
#include "PhoneLine.hpp"

PhoneLineLocker::PhoneLineLocker(PhoneLine *line, bool lock)
  : mPhoneLine(line)
{
  if(mPhoneLine && lock) {
    mPhoneLine->lock();
  }
}

PhoneLineLocker::~PhoneLineLocker()
{
  if(mPhoneLine) {
    mPhoneLine->unlock();
  }
}


