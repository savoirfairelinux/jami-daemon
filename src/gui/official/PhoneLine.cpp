
#include "PhoneLine.hpp"


PhoneLine::PhoneLine()
  : mCall(NULL)
{}

void 
PhoneLine::lock()
{
  mPhoneLineMutex.lock();
}

void 
PhoneLine::unlock()
{
  mPhoneLineMutex.unlock();
}
