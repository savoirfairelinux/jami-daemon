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
  , mLineStatus("test")
{}

PhoneLine::~PhoneLine()
{
  delete mCall;
  mCall = NULL;
}

QString
PhoneLine::getLineStatus()
{ 
  return mLineStatus;
}

void
PhoneLine::setLineStatus(const QString &status)
{ 
  mLineStatus = status;
  if(mSelected) {
    emit lineStatusChanged(mLineStatus);
  }
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
PhoneLine::select(bool hardselect)
{
  if(!mSelected) {
    _debug("PhoneLine %d: I am selected.\n", mLine);
    mSelected = true;

    if(!hardselect) {
      if(mCall) {
	if(mCall->isIncomming()) {
	  answer();
	}
	else {
	  unhold();
	}
      }
      else {
	setLineStatus("Ready.");
      }
    }

    emit selected();
  }
}

void 
PhoneLine::disconnect()
{
  mSelected = false;
  _debug("PhoneLine %d: I am disconnected.\n", mLine);
  if(mCall) {
    delete mCall;
    mCall = NULL;
  }

  emit unselected();
}

void
PhoneLine::unselect(bool hardselect)
{
  if(mSelected) {
    _debug("PhoneLine %d: I am unselected.\n", mLine);
    mSelected = false;
    if(mCall) {
      if(!hardselect) {
	mCall->hold();
      }
      emit backgrounded();
    }
    else {
      emit unselected();
    }
  }
}

void
PhoneLine::incomming(const Call &call)
{
  if(mCall) {
    _debug("PhoneLine %d: Trying to set a phone line to an active call.\n", mLine);
  }
  else {
    mCall = new Call(call);
    setLineStatus("Incomming...");
    emit backgrounded();
  }
}

void 
PhoneLine::clear()
{ 
  mBuffer.clear();
  emit bufferStatusChanged(mBuffer);
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
    if (QChar(c).isDigit()) {
      if(!mCall) {
	mSession.playDtmf(c);
	mBuffer += QString(c);
	emit bufferStatusChanged(mBuffer);
      }
      else {
	mCall->sendDtmf(c);
      }
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
PhoneLine::call(const QString &to) 
{
  _debug("PhoneLine %d: Calling %s.\n", mLine, to.toStdString().c_str());
  if(!mCall) {
    setLineStatus("Calling " + to + "...");
    mCall = new Call(mSession.createCall());
    mCall->call(to);
    clear();
  }
}

void 
PhoneLine::hold() 
{
  if(mCall) {
    setLineStatus("Holded.");
    _debug("PhoneLine %d: Trying to Hold.\n", mLine);
    mCall->hold();
  }

  unselect();
}

void 
PhoneLine::unhold() 
{
  if(mCall) {
    setLineStatus("Unholding...");
    _debug("PhoneLine %d: Trying to Unhold.\n", mLine);
    mCall->unhold();
  }
}

void 
PhoneLine::answer() 
{
  if(mCall) {
    setLineStatus("Answering...");
    _debug("PhoneLine %d: Trying to answer.\n", mLine);
    mCall->answer();
  }
}

void 
PhoneLine::hangup() 
{
  if(mCall) {
    setLineStatus("Hanguping...");
    _debug("PhoneLine %d: Trying to Hangup.\n", mLine);
    mCall->hangup();
    delete mCall;
    mCall = NULL;
  }
  else {
    clear();
  }

  //This is a hack for unselect;
  mSelected = true;
  unselect();
}


QString 
PhoneLine::getCallId()
{
  QString id;
  if(mCall) {
    id = mCall->id();
  }

  return id;
}

