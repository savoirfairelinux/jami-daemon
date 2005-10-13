#include <iostream>

#include "globals.h"
#include "Call.hpp"
#include "DebugOutput.hpp"
#include "PhoneLine.hpp"

PhoneLine::PhoneLine(const Session &session,
		     const Account &account,
		     unsigned int line)
  : mSession(session)
  , mAccount(account)
  , mCall(NULL)
  , mLine(line)
  , mSelected(false)
  , mLineStatus("test")
  , mActionTimer(new QTimer(this))
  , mIsOnError(false)
{
  QObject::connect(mActionTimer, SIGNAL(timeout()),
		   this, SLOT(resetAction()));
}

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
PhoneLine::resetAction()
{
  setAction("");
}

void
PhoneLine::setLineStatus(const QString &status)
{ 
  mActionTimer->stop();
  mAction = "";

  mLineStatus = status;
  if(mSelected) {
    emit lineStatusChanged(mLineStatus);
  }
}

void
PhoneLine::setAction(const QString &status)
{ 
  mActionTimer->stop();
  mAction = status;
  if(mSelected) {
    emit actionChanged(mAction);
  }
}

void
PhoneLine::setTempAction(const QString &status)
{ 
  mActionTimer->stop();
  mActionTimer->start(3000);
  mAction = status;
  if(mSelected) {
    emit actionChanged(mAction);
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
    DebugOutput::instance() << tr("PhoneLine %1: I am selected.\n").arg(mLine);
    mSelected = true;

    if(!hardselect) {
      if(mCall) {
	if(mIsOnError) {
	  close();
	}
	else if(mCall->isIncomming()) {
	  answer();
	}
	else {
	  unhold();
	}
      }
      else {
	setLineStatus(QObject::tr("Ready."));
	setAction("");
      }
    }

    emit selected();
  }

}

void 
PhoneLine::disconnect()
{
  mSelected = false;
  DebugOutput::instance() << QObject::tr("PhoneLine %1: I am disconnected.\n").arg(mLine);
  close();

  emit unselected();
}

void
PhoneLine::close()
{
  DebugOutput::instance() << tr("PhoneLine %1: I am closed.\n").arg(mLine);
  if(mCall) {
    delete mCall;
    mCall = NULL;
  }
  mIsOnError = false;
}

void
PhoneLine::error()
{
  mIsOnError = true;
}

void
PhoneLine::unselect(bool hardselect)
{
  if(mSelected) {
    DebugOutput::instance() << tr("PhoneLine %1: I am unselected.\n").arg(mLine);
    setAction("");
    mSelected = false;
    if(mIsOnError) {
      close();
    }
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
    DebugOutput::instance() << tr("PhoneLine %1: Trying to set a phone line to an active call.\n").arg(mLine);
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
  mBuffer = "";
  emit bufferStatusChanged(mBuffer);
}

void 
PhoneLine::sendKey(Qt::Key c)
{
  DebugOutput::instance() << tr("PhoneLine %1: Received the character:%2.\n")
    .arg(mLine)
    .arg(c);
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
	mBuffer += c;
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
  if(mBuffer.length()) {
    call(mBuffer);
  }
}

void 
PhoneLine::call(const QString &to) 
{
  DebugOutput::instance() << tr("PhoneLine %1: Calling %2.\n").arg(mLine).arg(to);
  if(!mCall) {
    setLineStatus(tr("Calling %1...").arg(to));
    mCall = new Call(mAccount.createCall(to));
    clear();
  }
}

void 
PhoneLine::hold() 
{
  if(mCall) {
      setAction(tr("Holding..."));
    DebugOutput::instance() << tr("PhoneLine %1: Trying to Hold.\n").arg(mLine);
    mCall->hold();
  }

  unselect();
}

void 
PhoneLine::unhold() 
{
  if(mCall) {
    setAction("Unholding...");
    DebugOutput::instance() << tr("PhoneLine %1: Trying to Unhold.\n").arg(mLine);
    mCall->unhold();
  }
}

void 
PhoneLine::answer() 
{
  if(mCall) {
    setAction("Answering...");
    DebugOutput::instance() << tr("PhoneLine %1: Trying to answer.\n").arg(mLine);
    mCall->answer();
  }
}

void 
PhoneLine::hangup(bool sendrequest) 
{
  if(sendrequest) {
    setAction(tr("Hanguping..."));
  }
  else {
    setAction(tr("Hanguped."));
  }

  if(mCall) {
    if(sendrequest) {
      mCall->hangup();
    }
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

