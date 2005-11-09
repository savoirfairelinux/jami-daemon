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

#include "globals.h"

#include "ConfigurationManager.hpp"
#include "Launcher.hpp"
#include "NumericKeypad.hpp"
#include "PhoneLine.hpp"
#include "PhoneLineButton.hpp"
#include "Requester.hpp"
#include "Session.hpp"
#include "SessionIOFactory.hpp"
#include "SFLLcd.hpp"
#include "SFLPhoneApp.hpp"
#include "SFLPhoneWindow.hpp"
#include "SFLRequest.hpp"
#include "TCPSessionIOCreator.hpp"
#include "VolumeControl.hpp"



SFLPhoneApp::SFLPhoneApp(int argc, char **argv)
  : QApplication(argc, argv)
  , mLauncher(new Launcher())
{
  mSettings.setPath("savoirfairelinux.com", PROGNAME);

  SessionIOFactory::instance().setCreator(new TCPSessionIOCreator(QString("localhost"), 3999));
  
  Session session;

  ConfigurationManager::instance().setSession(session);

  PhoneLineManager::instance().initialize(session);
  PhoneLineManager::instance().setNbLines(NB_PHONELINES);
  Requester::instance().registerDefaultObject< Request >();
  Requester::instance().registerObject< Request >(QString("playtone"));
  Requester::instance().registerObject< Request >(QString("stoptone"));
  Requester::instance().registerObject< Request >(QString("playdtmf"));

  Requester::instance().registerObject< ConfigGetAllRequest >(QString("configgetall"));
  Requester::instance().registerObject< ConfigSaveRequest >(QString("configsave"));
  Requester::instance().registerObject< StopRequest >(QString("stop"));


  Requester::instance().registerObject< EventRequest >(QString("getevents"));
  Requester::instance().registerObject< CallStatusRequest >(QString("getcallstatus"));
  Requester::instance().registerObject< PermanentRequest >(QString("answer"));
  Requester::instance().registerObject< PermanentRequest >(QString("notavailable"));
  Requester::instance().registerObject< PermanentRequest >(QString("refuse"));
  Requester::instance().registerObject< PermanentRequest >(QString("hangup"));
  Requester::instance().registerObject< TemporaryRequest >(QString("unmute"));
  Requester::instance().registerObject< TemporaryRequest >(QString("hold"));
  Requester::instance().registerObject< TemporaryRequest >(QString("unhold"));
  Requester::instance().registerObject< TemporaryRequest >(QString("senddtmf"));
  Requester::instance().registerObject< Request >(QString("setspkrvolume"));
  Requester::instance().registerObject< Request >(QString("setmicvolume"));
  Requester::instance().registerObject< Request >(QString("mute"));

  mKeypad = new NumericKeypad();
}

void
SFLPhoneApp::launch()
{
  if(mLauncher) {
    mLauncher->start();
  }
}

void
SFLPhoneApp::initConnections(SFLPhoneWindow *w)
{
  // We connect the phone line buttons to the PhoneLineManager
  unsigned int i = 0;
  for(std::list< PhoneLineButton * >::iterator pos = w->mPhoneLineButtons.begin();
      pos != w->mPhoneLineButtons.end();
      pos++) {
    PhoneLine *line = PhoneLineManager::instance().getPhoneLine(i);
    QObject::connect(*pos, SIGNAL(clicked(unsigned int)),
		     &PhoneLineManager::instance(), SLOT(selectLine(unsigned int)));
    QObject::connect(line, SIGNAL(selected()),
		     *pos, SLOT(press()));
    QObject::connect(line, SIGNAL(unselected()),
		     *pos, SLOT(release()));
    QObject::connect(line, SIGNAL(backgrounded()),
		     *pos, SLOT(suspend()));
    QObject::connect(line, SIGNAL(peerUpdated(QString)),
		     *pos, SLOT(setToolTip(QString)));
    QObject::connect(line, SIGNAL(peerCleared()),
		     *pos, SLOT(clearToolTip()));

    i++;
  }


  QObject::connect(w, SIGNAL(needRegister()),
		   &PhoneLineManager::instance(), SLOT(registerToServer()));
  //QObject::connect(&PhoneLineManager::instance(), SIGNAL(registered()),
  //		   w, SIGNAL(registered()));
  QObject::connect(w->mOk, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(call()));
  QObject::connect(w->mMute, SIGNAL(clicked(bool)),
		   &PhoneLineManager::instance(), SLOT(mute(bool)));
  QObject::connect(w->mDtmf, SIGNAL(clicked(bool)),
		   mKeypad, SLOT(setShown(bool)));
  QObject::connect(mKeypad, SIGNAL(hidden()),
		   w->mDtmf, SLOT(release()));
  QObject::connect(w->mSetup, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(setup()));
  QObject::connect(w->mHangup, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(hangup()));
  QObject::connect(w->mHold, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(hold()));
  QObject::connect(w->mClear, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(clear()));
  QObject::connect(w->mTransfer, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(transfer()));
  QObject::connect(w, SIGNAL(keyPressed(Qt::Key)),
		   &PhoneLineManager::instance(), SLOT(sendKey(Qt::Key)));


  // Keypad connections
  QObject::connect(mKeypad, SIGNAL(keyPressed(Qt::Key)),
		   &PhoneLineManager::instance(), SLOT(sendKey(Qt::Key)));
  
  // LCD Connections.
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(lineStatusSet(QString)),
		   w->mLcd, SLOT(setLineStatus(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(unselectedLineStatusSet(QString)),
		   w->mLcd, SLOT(setUnselectedLineStatus(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(actionSet(QString)),
		   w->mLcd, SLOT(setAction(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(globalStatusSet(QString)),
		   w->mLcd, SLOT(setGlobalStatus(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(bufferStatusSet(QString)),
		   w->mLcd, SLOT(setBufferStatus(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(talkingStarted(QTime)),
		   w->mLcd, SLOT(setLineTimer(QTime)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(talkingStopped()),
		   w->mLcd, SLOT(clearLineTimer()));


  //Volume connections
  QObject::connect(w->mVolume, SIGNAL(valueUpdated(int)),
		   &PhoneLineManager::instance(), SLOT(setVolume(int)));
  QObject::connect(w->mMicVolume, SIGNAL(valueUpdated(int)),
		   &PhoneLineManager::instance(), SLOT(setMicVolume(int)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(volumeUpdated(int)),
		   w->mVolume, SLOT(setValue(int)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(micVolumeUpdated(int)),
		   w->mMicVolume, SLOT(setValue(int)));


  //Line events connections
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(disconnected()),
		   w, SLOT(askReconnect()));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(disconnected()),
		   w, SLOT(show()));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(connected()),
		   w, SLOT(show()));
  QObject::connect(w, SIGNAL(reconnectAsked()),
		   &PhoneLineManager::instance(), SLOT(connect()));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(stopped()),
		   w, SLOT(close()));
  QObject::connect(w, SIGNAL(needToCloseDaemon()),
		   &PhoneLineManager::instance(), SLOT(stop()));

  //sflphoned launch
  QObject::connect(w, SIGNAL(launchAsked()),
		   mLauncher, SLOT(start()));
  QObject::connect(mLauncher, SIGNAL(error()),
		   w, SLOT(askLaunch()));
  QObject::connect(mLauncher, SIGNAL(started()),
		   &PhoneLineManager::instance(), SLOT(connect()));

  QObject::connect(&PhoneLineManager::instance(), SIGNAL(gotErrorOnCallStatus(QString)),
		   w, SLOT(askResendStatus(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(gotErrorOnGetEvents(QString)),
		   w, SLOT(askResendStatus(QString)));
  QObject::connect(w, SIGNAL(resendStatusAsked()),
		   &PhoneLineManager::instance(), SIGNAL(readyToSendStatus()));

  
  //Configuration events.
  QObject::connect(&ConfigurationManager::instance(), SIGNAL(updated()),
		   w, SLOT(showSetup()));
  QObject::connect(&ConfigurationManager::instance(), SIGNAL(ringtonesUpdated()),
		   w, SIGNAL(ringtonesUpdated()));
  QObject::connect(&ConfigurationManager::instance(), SIGNAL(audioDevicesUpdated()),
		   w, SIGNAL(audioDevicesUpdated()));
  //QObject::connect(&ConfigurationManager::instance(), SIGNAL(saved()),
  //		   w, SLOT(hideSetup()));
}

