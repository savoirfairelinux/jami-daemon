#include "PhoneLineLocker.hpp"
#include "PhoneLine.hpp"

PhoneLineLocker::PhoneLineLocker(PhoneLine *line)
  : mPhoneLine(line)
{
  mPhoneLine->lock();
}

PhoneLineLocker::~PhoneLineLocker()
{
  mPhoneLine->unlock();
}


