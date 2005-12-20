/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#include <stdexcept>

#include "globals.h"

#include "CallStatusFactory.hpp"
#include "ConfigurationManager.hpp"
#include "SFLEvents.hpp"
#include "SFLCallStatus.hpp"
#include "PhoneLine.hpp"
#include "PhoneLineLocker.hpp"
#include "PhoneLineManager.hpp"
#include "Request.hpp"

#include <qmessagebox.h>

PhoneLineManagerImpl::PhoneLineManagerImpl()
  : mSession(NULL)
  , mAccount(NULL)
  , mCurrentLine(NULL)
  , mIsInitialized(false)
  , mVolume(-1)
  , mMicVolume(-1)
  , mIsConnected(false)
  , mIsStopping(false)
  , mLastNumber("")
{
  EventFactory::instance().registerDefaultEvent< DefaultEvent >();
  // TODO: 000
  EventFactory::instance().registerEvent< CallRelatedEvent >("000");
  EventFactory::instance().registerEvent< IncommingEvent >("001");
  EventFactory::instance().registerEvent< HangupEvent >("002");
  // TODO: 020
  EventFactory::instance().registerEvent< LoadSetupEvent >("010");
  EventFactory::instance().registerEvent< CallRelatedEvent >("020");
  EventFactory::instance().registerEvent< VolumeEvent >("021");
  EventFactory::instance().registerEvent< MicVolumeEvent >("022");
  EventFactory::instance().registerEvent< MessageTextEvent >("030");
  EventFactory::instance().registerEvent< TryingStatus >("110");
  EventFactory::instance().registerEvent< RingingStatus >("111");
  EventFactory::instance().registerEvent< HoldStatus >("112");
  EventFactory::instance().registerEvent< EstablishedStatus >("113");
  EventFactory::instance().registerEvent< BusyStatus >("114");
  EventFactory::instance().registerEvent< CongestionStatus >("115");
  EventFactory::instance().registerEvent< WrongNumberStatus >("116");
  QObject::connect(this, SIGNAL(disconnected()),
		   this, SLOT(closeSession()));
  QObject::connect(this, SIGNAL(readyToHandleEvents()),
		   this, SLOT(handleEvents()));
  QObject::connect(this, SIGNAL(connected()),
		   this, SIGNAL(readyToSendStatus()));
  QObject::connect(this, SIGNAL(readyToSendStatus()),
		   this, SLOT(startSession()));
  
}

PhoneLineManagerImpl::~PhoneLineManagerImpl()
{
  delete mSession;
  delete mAccount;
  for(std::vector< PhoneLine * >::iterator pos = mPhoneLines.begin();
      pos != mPhoneLines.end();
      pos++) {
    delete *pos;
  }
}


void
PhoneLineManagerImpl::hasDisconnected()
{
  if(!mIsStopping) {
    emit disconnected();
  }
  else {
    emit stopped();
  }
}

void
PhoneLineManagerImpl::initialize(const Session &session)
{
  if(!mIsInitialized) {
    mIsInitialized = true;
    mSession = new Session(session);
    mAccount = new Account(mSession->getDefaultAccount());
  }
}

void PhoneLineManagerImpl::isInitialized()
{
  if(!mIsInitialized) {
    throw std::logic_error("Trying to use PhoneLineManager without prior initialize.");
  }
}

void
PhoneLineManagerImpl::connect()
{
  isInitialized();

  emit globalStatusSet(QString(tr("Trying to connect to sflphone server..")));
  mSession->connect();
}

void
PhoneLineManagerImpl::registerToServer()
{
  isInitialized();
  
  Request *r = mSession->registerToServer();
  QObject::connect(r, SIGNAL(success()),
		   this, SIGNAL(registered()));
}

void
PhoneLineManagerImpl::stop()
{
  isInitialized();

  emit globalStatusSet(QString(tr("Stopping sflphone server..")));
  mIsStopping = true;
  if(mIsConnected) {
    mSession->stop();
  }
  else {
    emit stopped();
  }
}

void 
PhoneLineManagerImpl::startSession()
{
  isInitialized();

  closeSession();

  mIsConnected = true;
  emit globalStatusSet(QString(tr("Trying to get line status...")));
  mSession->getCallStatus();
}

void 
PhoneLineManagerImpl::handleEvents()
{
  isInitialized();

  emit globalStatusSet(QString(tr("Welcome to SFLPhone")));
  mSession->getEvents();

  Request *r;
  r = mSession->list("ringtones");
  QObject::connect(r, SIGNAL(parsedEntry(QString, QString, QString, QString, QString)),
		   &ConfigurationManager::instance(), SLOT(addRingtone(QString, QString)));
  QObject::connect(r, SIGNAL(success(QString, QString)),
		   &ConfigurationManager::instance(), SIGNAL(ringtonesUpdated()));
  
  r = mSession->list("audiodevice");
  QObject::connect(r, SIGNAL(parsedEntry(QString, QString, QString, QString, QString)),
		   &ConfigurationManager::instance(), SLOT(addAudioDevice(QString, 
									  QString,
									  QString)));
  QObject::connect(r, SIGNAL(success(QString, QString)),
		   &ConfigurationManager::instance(), SIGNAL(audioDevicesUpdated()));

  r = mSession->list("codecdescriptor");
  QObject::connect(r, SIGNAL(parsedEntry(QString, QString, QString, QString, QString)),
		   &ConfigurationManager::instance(), SLOT(addCodec(QString, QString)));
  QObject::connect(r, SIGNAL(success(QString, QString)),
		   &ConfigurationManager::instance(), SIGNAL(codecsUpdated()));

  emit handleEventsSent();
}


void 
PhoneLineManagerImpl::closeSession()
{
  isInitialized();

  mCurrentLine = NULL;
  mIsConnected = false;
    
  unsigned int i = 0;
  while(i < mPhoneLines.size()) {
    mPhoneLines[i]->disconnect();
    i++;
  }

  emit lineStatusSet("");
  emit bufferStatusSet("");
  emit actionSet("");
  emit globalStatusSet("Disconnected.");
}


PhoneLine *
PhoneLineManagerImpl::getCurrentLine()
{
  isInitialized();

  return mCurrentLine;
}

void 
PhoneLineManagerImpl::setNbLines(unsigned int nb)
{
  isInitialized();

  mPhoneLines.clear();
  for(unsigned int i = 0; i < nb; i++) {
    PhoneLine *p = new PhoneLine(*mSession, *mAccount, i + 1);
    QObject::connect(p, SIGNAL(lineStatusChanged(QString)),
		     this, SIGNAL(unselectedLineStatusSet(QString)));
    mPhoneLines.push_back(p);
  }
}

PhoneLine *
PhoneLineManagerImpl::getNextAvailableLine()
{
  isInitialized();

  PhoneLine *selectedLine = NULL;
  PhoneLine *current = mCurrentLine;
    

  unsigned int i = 0;
  while(i < mPhoneLines.size() && !selectedLine) {
    if(mPhoneLines[i]->isAvailable() && 
       mPhoneLines[i] != current) {
      selectedLine = mPhoneLines[i];
    }
    else {
      i++;
    }
  }

  return selectedLine;
}

PhoneLine *
PhoneLineManagerImpl::getLine(const Call &call)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;

  unsigned int i = 0;
  while(i < mPhoneLines.size() && !selectedLine) {
    if(mPhoneLines[i]->getCallId() == call.id()) {
      selectedLine = mPhoneLines[i];
    }
    else {
      i++;
    }
  }

  return selectedLine;
}

PhoneLine *
PhoneLineManagerImpl::getLine(unsigned int line)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;

  if(line < mPhoneLines.size()) {
    selectedLine = mPhoneLines[line];
  }

  return selectedLine;
}

void
PhoneLineManagerImpl::select(PhoneLine *line, bool hardselect)
{
  if(line && (mCurrentLine != line)) {
    unselect();
    
    QObject::disconnect(line, SIGNAL(lineStatusChanged(QString)),
			this, SIGNAL(unselectedLineStatusSet(QString)));
    QObject::connect(line, SIGNAL(lineStatusChanged(QString)),
		     this, SIGNAL(lineStatusSet(QString)));
    QObject::connect(line, SIGNAL(actionChanged(QString)),
		     this, SIGNAL(actionSet(QString)));
    QObject::connect(line, SIGNAL(bufferStatusChanged(QString)),
		     this, SIGNAL(bufferStatusSet(QString)));
    QObject::connect(line, SIGNAL(talkingStarted(QTime)),
		     this, SIGNAL(talkingStarted(QTime)));
    QObject::connect(line, SIGNAL(talkingStopped()),
		     this, SIGNAL(talkingStopped()));
    QObject::connect(line, SIGNAL(transfered()),
		     this, SLOT(unselect()));
    
    
    mCurrentLine = line;
    mCurrentLine->select(hardselect);
    if(mCurrentLine->isAvailable() && !hardselect) {
      mSession->playTone();
    }
    if(mCurrentLine->isTalking()) {
      emit talkingStarted(mCurrentLine->getTalkingTime());
    }
    emit lineStatusSet(mCurrentLine->getLineStatus());
    emit bufferStatusSet(mCurrentLine->getBuffer());
  }
}

void
PhoneLineManagerImpl::unselect()
{
  if(mCurrentLine) {
    QObject::disconnect(mCurrentLine, SIGNAL(lineStatusChanged(QString)),
			this, SIGNAL(lineStatusSet(QString)));
    QObject::disconnect(mCurrentLine, SIGNAL(actionChanged(QString)),
			this, SIGNAL(actionSet(QString)));
    QObject::disconnect(mCurrentLine, SIGNAL(bufferStatusChanged(QString)),
			this, SIGNAL(bufferStatusSet(QString)));
    QObject::disconnect(mCurrentLine, SIGNAL(talkingStarted(QTime)),
			this, SIGNAL(talkingStarted(QTime)));
    QObject::disconnect(mCurrentLine, SIGNAL(talkingStopped()),
			this, SIGNAL(talkingStopped()));
    QObject::disconnect(mCurrentLine, SIGNAL(transfered()),
		     this, SLOT(unselect()));
    QObject::connect(mCurrentLine, SIGNAL(lineStatusChanged(QString)),
		     this, SIGNAL(unselectedLineStatusSet(QString)));
    if(mCurrentLine->isAvailable()) {
      mSession->stopTone();
    }
    mCurrentLine->unselect();
    mCurrentLine = NULL;

    emit lineStatusSet("");
    emit actionSet("");
    emit bufferStatusSet("");
    emit talkingStopped();
  }
}

PhoneLine *
PhoneLineManagerImpl::selectNextAvailableLine()
{
  isInitialized();

  PhoneLine *selectedLine = getNextAvailableLine();

  // If we found one available line.
  if(selectedLine) {
    unselect();
    select(selectedLine);
  }

  return selectedLine;
}



PhoneLine *
PhoneLineManagerImpl::getPhoneLine(unsigned int line)
{
  isInitialized();

  if(mPhoneLines.size() <= line) {
    throw std::runtime_error("Trying to get an invalid Line");
  }
  
  return mPhoneLines[line];
}

PhoneLine *
PhoneLineManagerImpl::getPhoneLine(const QString &callId)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;

  unsigned int i = 0;
  while(i < mPhoneLines.size() &&
	!selectedLine) {
    if(mPhoneLines[i]->getCallId() == callId) {
      selectedLine = mPhoneLines[i];
    }
    else {
      i++;
    }
  }
  
  return selectedLine;
}


void
PhoneLineManagerImpl::sendKey(Qt::Key c)
{
  isInitialized();

  PhoneLine *selectedLine = getCurrentLine();

  // Only digits that select a line if there's
  // no current line.
  switch(c) {
  case Qt::Key_F1:
  case Qt::Key_F2:
  case Qt::Key_F3:
  case Qt::Key_F4:
  case Qt::Key_F5:
  case Qt::Key_F6:
    selectLine(c - Qt::Key_F1);
    break;
    
  default:
    if (!selectedLine) {
      selectedLine = selectNextAvailableLine();
    }
    
    if(selectedLine) {
      if (c == Qt::Key_Enter || c == Qt::Key_Return) {
        mLastNumber = selectedLine->getBuffer();
      }
      selectedLine->sendKey(c);
    }
  }
}


void 
PhoneLineManagerImpl::selectLine(const QString &callId, bool hardselect)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;
  unsigned int line = 0;
  while(!selectedLine && line < mPhoneLines.size()) {
    if(mPhoneLines[line]->getCallId() == callId) {
      selectedLine = mPhoneLines[line];
    }
    else {
      line++;
    }
  }

  if(selectedLine) {
    selectLine(line, hardselect);
  }
  else {
    DebugOutput::instance() << QObject::tr("PhoneLineManager: Tried to selected line with call ID (%1), "
					   "which appears to be invalid.\n").arg(callId);
  }
}

void
PhoneLineManagerImpl::unselectLine(unsigned int line)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;
  // getting the wanted line;
  {
    if(mPhoneLines.size() > line) {
      selectedLine = mPhoneLines[line];
    }
  }
   
  if(selectedLine == mCurrentLine) {
    unselect();
  }
}

/**
 * Warning: This function might 'cause a problem if
 * we select 2 line in a very short time.
 */
void
PhoneLineManagerImpl::selectLine(unsigned int line, bool hardselect)
{
  isInitialized();

  PhoneLine *selectedLine = NULL;
  // getting the wanted line;
  {
    if(mPhoneLines.size() > line) {
      selectedLine = mPhoneLines[line];
    }
  }
   
  if(selectedLine != NULL) {
    PhoneLine *oldLine = mCurrentLine;

    if(oldLine != selectedLine) {
      select(selectedLine, hardselect);
    }
  }
  else {
    DebugOutput::instance() << QObject::tr("PhoneLineManager: Tried to selected line %1, "
					   "which appears to be invalid.\n").arg(line);
  }
}

void
PhoneLineManagerImpl::call(const QString &to)
{
  PhoneLine *current = getCurrentLine();
  if(current) {
    current->call(to);
    mLastNumber = to;
  }
}


void
PhoneLineManagerImpl::call()
{
  PhoneLine *current = getCurrentLine();
  if(current) {
    mLastNumber = current->getBuffer();
    current->call();
  }
}

void
PhoneLineManagerImpl::makeNewCall(const QString& to) 
{
  selectNextAvailableLine();
  call(to);
}


void
PhoneLineManagerImpl::transfer() 
{
  if(mCurrentLine) {
    mCurrentLine->transfer();
  }
}



void
PhoneLineManagerImpl::hold()
{
  PhoneLine *selectedLine = mCurrentLine;
  mCurrentLine = NULL;

  if(selectedLine) {
    if(selectedLine->isAvailable()) {
      mSession->stopTone();
    }
    selectedLine->hold();
  }
}

void
PhoneLineManagerImpl::redial()
{
  PhoneLine *phoneLine = selectNextAvailableLine();
  if(phoneLine && !mLastNumber.isEmpty()) {
    phoneLine->call(mLastNumber);
  }
}

void
PhoneLineManagerImpl::hangup(bool sendrequest)
{
  PhoneLine *selectedLine = mCurrentLine;
  mCurrentLine = NULL;

  if(selectedLine) {
    if(selectedLine->isAvailable()) {
      mSession->stopTone();
    }
    selectedLine->hangup(sendrequest);
    lineStatusSet("");
  }
}

void
PhoneLineManagerImpl::mute(bool muting)
{
  if(muting) {
    mute();
  }
  else {
    unmute();
  }
}

void
PhoneLineManagerImpl::mute()
{
  isInitialized();
  
  mSession->mute();
}

void
PhoneLineManagerImpl::setup()
{
  isInitialized();
  
  mSession->configGetAll();
}

void
PhoneLineManagerImpl::unmute()
{
  isInitialized();
  
  mSession->unmute();
}

void
PhoneLineManagerImpl::hangup(const QString &callId, bool sendrequest)
{
  PhoneLine *selectedLine = getPhoneLine(callId);
  hangup(selectedLine, sendrequest);
}
 
void
PhoneLineManagerImpl::hangup(unsigned int line, bool sendrequest)
{
  PhoneLine *selectedLine = getPhoneLine(line);
  hangup(selectedLine, sendrequest);
}
 
void 
PhoneLineManagerImpl::hangup(PhoneLine *line, bool sendrequest)
{
  if(line) {
    line->hangup(sendrequest);
    if(mCurrentLine == line) {
      unselect();
    }
  }
}

void
PhoneLineManagerImpl::clear()
{
  PhoneLine *selectedLine = mCurrentLine;

  if(selectedLine) {
    selectedLine->clear();
  }
}

void 
PhoneLineManagerImpl::incomming(const QString &accountId,
				const QString &callId,
				const QString &peer)
{
  Call call(mSession->id(), accountId, callId, peer, true);
  PhoneLine *line = addCall(call, QObject::tr("Incomming"));
  if(line) {
    line->setLineStatus(QObject::tr("Ringing (%1)...").arg(peer));
  }
}

PhoneLine *
PhoneLineManagerImpl::addCall(const QString &accountId,
			      const QString &callId,
			      const QString &peer,
			      const QString &state)
{
  return addCall(Call(mSession->id(), accountId, callId, peer), state);
}

PhoneLine *
PhoneLineManagerImpl::addCall(Call call,
			      const QString &state)
{
  PhoneLine *selectedLine = getNextAvailableLine();

  if(selectedLine) {
    selectedLine->incomming(call);
    selectedLine->setLineStatus(state);
  }
  else {
    DebugOutput::instance() << QObject::tr("PhoneLineManager: There's no available lines"
					     "here for the incomming call ID: %1.\n")
      .arg(call.id());
    call.notAvailable();
  }

  return selectedLine;
}

void
PhoneLineManagerImpl::updateVolume(int volume)
{
  mVolume = volume;
  emit volumeUpdated((unsigned int)volume);
}

void
PhoneLineManagerImpl::updateMicVolume(int volume)
{
  mMicVolume = volume;
  emit micVolumeUpdated((unsigned int)volume);
}

void 
PhoneLineManagerImpl::setVolume(int volume)
{
  if(mVolume != volume) {
    mSession->volume(volume);
    updateVolume(volume);
  }
}

void 
PhoneLineManagerImpl::setMicVolume(int volume)
{
  if(mMicVolume != volume) {
    mSession->micVolume(volume);
    updateMicVolume(volume);
  }
}

void
PhoneLineManagerImpl::incomingMessageText(const QString& message) 
{
  QMessageBox messageBox;
  messageBox.setText(message);
  messageBox.exec();
}


