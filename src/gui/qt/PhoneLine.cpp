/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *                                                                              
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                              
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <iostream>

#include "globals.h"
#include "Call.hpp"
#include "DebugOutput.hpp"
#include "PhoneLine.hpp"
#include "Request.hpp"

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
  , mTalking(false)
  , mIsOnError(false)
  , mIsTransfering(false)
{
  QObject::connect(mActionTimer, SIGNAL(timeout()),
		   this, SLOT(resetAction()));
  QObject::connect(this, SIGNAL(transfered()),
		   this, SLOT(finishTransfer()));
}

PhoneLine::~PhoneLine()
{
  clearCall();
}

void 
PhoneLine::clearCall() 
{
  if(mCall) {
    delete mCall;
    mCall = NULL;
  }

  clearPeer();
}

void 
PhoneLine::setCall(const Call &call) 
{
  setCall(new Call(call));
}

void 
PhoneLine::setCall(Call *call) 
{
  clearCall();

  mCall = call;
  setPeer(mCall->peer());
}


void 
PhoneLine::setPeer(const QString &peer)
{
  mPeer = peer;
  emit peerUpdated(peer);
}

void
PhoneLine::clearPeer()
{
  mPeer = "";
  emit peerCleared();
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
PhoneLine::setLineStatus(QString status)
{ 
  mActionTimer->stop();
  mAction = "";

  mLineStatus = status;
  emit lineStatusChanged(mLineStatus);
}

void
PhoneLine::setAction(QString status)
{ 
  mActionTimer->stop();
  mAction = status;
  emit actionChanged(mAction);
}

void
PhoneLine::setTempAction(QString status)
{ 
  mActionTimer->stop();
  mActionTimer->start(3000);
  mAction = status;
  emit actionChanged(mAction);
}

unsigned int 
PhoneLine::line()
{
  return mLine;
}

void 
PhoneLine::lock()
{
  //mPhoneLineMutex.lock();
}

void 
PhoneLine::unlock()
{
  //mPhoneLineMutex.unlock();
}

void 
PhoneLine::select(bool hardselect)
{
  if(!mSelected) {
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
  close();

  emit unselected();
}

void
PhoneLine::close()
{
  clearCall();
  mIsOnError = false;
}

void
PhoneLine::error(QString message)
{
  setLineStatus(message);
  mIsOnError = true;
}

void
PhoneLine::unselect(bool hardselect)
{
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

void
PhoneLine::incomming(const Call &call)
{
  if(mCall) {
    DebugOutput::instance() << tr("PhoneLine %1: Trying to set a phone line to an active call.\n").arg(mLine);
  }
  else {
    setCall(call);
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
    if(mCall && mIsTransfering) {
      return transfer();
    }
    break;

  case Qt::Key_Backspace:
    if((!mCall || mIsTransfering) && mBuffer.length() > 0) {
      mBuffer.remove(mBuffer.length() - 1, 1);
      emit bufferStatusChanged(mBuffer);
    }
    break;

  default:
    if(!mCall || mIsTransfering) {
      mBuffer += c;
      emit bufferStatusChanged(mBuffer);
    }

    if (QChar(c).isDigit() || c == Qt::Key_Asterisk || c == Qt::Key_NumberSign) {
      if(!mCall) {
	mSession.playDtmf(c);
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
    Call *call;
    Request *r = mAccount.createCall(call, to);
    // entry
    connect(r, SIGNAL(entry(QString, QString)),
	    this, SLOT(setLineStatus(QString)));

    connect(r, SIGNAL(error(QString, QString)),
	    this, SLOT(error(QString)));

    connect(r, SIGNAL(success(QString, QString)),
	    this, SLOT(setTalkingState()));

    setCall(call);
    clear();
  }
}

void
PhoneLine::setTalkingState()
{
  mTalking = true;
  mTalkingTime.start();
  talkingStarted(mTalkingTime);
  setLineStatus(tr("Talking to: %1").arg(mPeer));
  setAction("");
}

void
PhoneLine::transfer()
{
  if(mCall) {
    if(mBuffer.length() == 0) {
      DebugOutput::instance() << tr("PhoneLine %1: We're now in transfer mode.\n");
      setAction(tr("Transfer to:"));
      clear();
      unselect();
      mIsTransfering = true;
    }
    else {
      DebugOutput::instance() << tr("PhoneLine %1: Trying to transfer to \"%2\".\n")
	.arg(mLine)
	.arg(mBuffer);
      connect(mCall->transfer(mBuffer), SIGNAL(success(QString, QString)),
	      this, SIGNAL(transfered()));
      clear();

      unselect(true);
    }
  }
}

void 
PhoneLine::finishTransfer()
{
  clearCall();
  stopTalking();

  if(mIsTransfering) {
    mIsTransfering = false;
    emit transfered();
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
PhoneLine::stopTalking()
{
  mTalking = false;
  emit talkingStopped();
}

void 
PhoneLine::hangup(bool sendrequest) 
{
  stopTalking();

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
    clearCall();
  }

  clear();
  clearPeer();
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

