#include "globals.h"
#include "PhoneLine.hpp"
#include "Call.hpp"

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

void 
PhoneLine::select()
{
  _debug("I am selected.\n");
}

void
PhoneLine::unselect()
{
  _debug("I am unselected.\n");
}
