#include <iostream>

#include "globals.h"
#include "PhoneLine.hpp"
#include "Call.hpp"

PhoneLine::PhoneLine(const Call &call,
		     unsigned int line)
  : mCall(call)
  , mLine(line)
  , mSelected(false)
  , mInUse(false)
{}

unsigned int 
PhoneLine::line()
{
  return mLine;
}

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
  if(!mSelected) {
    _debug("PhoneLine %d: I am selected.\n", mLine + 1);
    mSelected = true;
    emit selected();
  }
}

void
PhoneLine::unselect()
{
  if(mSelected) {
    _debug("PhoneLine %d: I am unselected.\n", mLine + 1);
    mSelected = false;
    if(!mInUse) {
      mBuffer.clear();
      emit backgrounded();
    }
    else {
      emit unselected();
    }
  }
}

void 
PhoneLine::sendKey(Qt::Key c)
{
  _debug("PhoneLine %d: Received the character:%s.\n", mLine + 1, QString(c).toStdString().c_str());
  switch(c) {
  case Qt::Key_Enter:
  case Qt::Key_Return:
    if(!mInUse) {
      return call();
    }
    break;

  default:
    if(!mInUse) {
      mBuffer += QString(c).toStdString();
    }
  }
}

void
PhoneLine::call()
{
  call(mBuffer);
}

void 
PhoneLine::call(const std::string &to) 
{
  _debug("PhoneLine %d: Calling %s.\n", mLine, to.c_str());
  if(!mInUse) {
    mInUse = true;
    mCall.call(to);
  }
}
