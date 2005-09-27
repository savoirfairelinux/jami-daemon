#include <iostream>

#include "globals.h"
#include "PhoneLine.hpp"
#include "Call.hpp"

PhoneLine::PhoneLine(const Session &session,
		     unsigned int line)
  : mSession(session)
  , mCall(NULL)
  , mLine(line)
  , mSelected(false)
{}

PhoneLine::~PhoneLine()
{
  delete mCall;
  mCall = NULL;
}

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
    _debug("PhoneLine %d: I am selected.\n", mLine);
    mSelected = true;

    if(mCall) {
      if(mCall->isIncomming()) {
	mCall->answer();
      }
      else {
	mCall->unhold();
      }
    }

    emit selected();
  }
}

void
PhoneLine::unselect()
{
  if(mSelected) {
    _debug("PhoneLine %d: I am unselected.\n", mLine);
    mSelected = false;
    if(mCall) {
      mCall->hold();
      emit backgrounded();
    }
    else {
      clear();
      emit unselected();
    }
  }
}

void
PhoneLine::incomming(const Call &call)
{
  if(mCall) {
    _debug("PhoneLine %d: Trying to set an incomming call to an active call.\n", mLine);
  }
  else {
    mCall = new Call(call);
    emit backgrounded();
  }
}

void 
PhoneLine::clear()
{ 
  mBuffer.clear();
}

void 
PhoneLine::sendKey(Qt::Key c)
{
  _debug("PhoneLine %d: Received the character:%s.\n", 
	 mLine, 
	 QString(c).toStdString().c_str());
  switch(c) {
  case Qt::Key_Enter:
  case Qt::Key_Return:
    if(!mCall) {
      return call();
    }
    break;

  default:
    if(!mCall) {
      mSession.playDtmf(c);
      mBuffer += QString(c).toStdString();
    }
    else {
      mCall->sendDtmf(c);
    }
  }
}

void
PhoneLine::call()
{
  if(mBuffer.size()) {
    call(mBuffer);
  }
}

void 
PhoneLine::call(const std::string &to) 
{
  _debug("PhoneLine %d: Calling %s.\n", mLine, to.c_str());
  if(!mCall) {
    mCall = new Call(mSession.createCall());
    mCall->call(to);
  }
}

void 
PhoneLine::hold() 
{
  if(mCall) {
    _debug("PhoneLine %d: Trying to Hold.\n", mLine);
    mCall->hold();
  }

  unselect();
}

void 
PhoneLine::hangup() 
{
  if(mCall) {
    _debug("PhoneLine %d: Trying to Hangup.\n", mLine);
    mCall->hangup();
    delete mCall;
    mCall = NULL;
  }

  //This is a hack for unselect;
  mSelected = true;
  unselect();
}


std::string 
PhoneLine::getCallId()
{
  std::string id;
  if(mCall) {
    id = mCall->id();
  }

  return id;
}
