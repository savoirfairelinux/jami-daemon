/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
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

#include "SFLPhoneWindow.hpp"

#include <qbitmap.h>

//To test if we are in QT4
#ifdef QT3_SUPPORT 
#include <QIcon>
#endif


#include <qlabel.h>
#include <qmessagebox.h>
#include <qevent.h>
#include <qpixmap.h>
#include <iostream>

#include "globals.h"
#include "JPushButton.hpp"
#include "PhoneLineButton.hpp"
#include "SFLLcd.hpp"
#include "VolumeControl.hpp"
#include "NumericKeypad.hpp"

#define LOGO_IMAGE "logo_ico.png"
#define BACKGROUND_IMAGE "main.png"
#define VOLUME_IMAGE "volume.png"

			    
SFLPhoneWindow::SFLPhoneWindow()
#ifdef QT3_SUPPORT
  : QMainWindow(NULL, Qt::FramelessWindowHint)
#else
    : QMainWindow(NULL, NULL, 
		  Qt::WDestructiveClose | 
		  Qt::WStyle_Customize | 
		  Qt::WStyle_NoBorder)
#endif
{
  mLastWindowPos = pos();
  mSetupPanel = new ConfigurationPanel(this, "ConfigurationPanel");
  mKeypad = new NumericKeypad(NULL, false);
  mKeypad->setWindowReference(this);
  

  connect(this, SIGNAL(ringtonesUpdated()),      mSetupPanel, SLOT(updateRingtones()));
  connect(this, SIGNAL(audioDevicesUpdated()),   mSetupPanel, SLOT(updateAudioDevices()));
  connect(this, SIGNAL(audioDevicesInUpdated()), mSetupPanel, SLOT(updateAudioDevicesIn()));
  connect(this, SIGNAL(audioDevicesOutUpdated()),mSetupPanel, SLOT(updateAudioDevicesOut()));
  connect(this, SIGNAL(codecsUpdated()),         mSetupPanel, SLOT(updateCodecs()));

  connect(mSetupPanel, SIGNAL(needRegister(const QString&)), this, SIGNAL(needRegister(const QString&)));
  connect(this, SIGNAL(registerReturn(bool, QString)),  mSetupPanel, SLOT(slotRegisterReturn(bool, QString)));
  
  // when we receive a signal from mSetupPanel, we should resend one to...
  connect(mSetupPanel, SIGNAL(soundDriverChanged()), this, SIGNAL(soundDriverChanged()));

  // we are an intermediate... 
  connect(this, SIGNAL(testSoundDriverReturn(bool, QString)),  mSetupPanel, SLOT(slotSoundDriverReturn(bool, QString)));

  // Initialize the background image
  setName("main");

  mPaintTimer = new QTimer(this);
  connect(mPaintTimer, SIGNAL(timeout()),
	  this, SLOT(delayedPaint()));
  mPaintTimer->start(50);
  


  QPixmap logo(QPixmap::fromMimeSource(LOGO_IMAGE));
#ifdef QIcon
  setWindowIcon(QIcon(logo));
#else
  setIcon(logo);
#endif

  mLastPos = pos();
  
  initGUIButtons();
  initWindowButtons();
  initLineButtons();
  initLCD();
}

SFLPhoneWindow::~SFLPhoneWindow()
{}

void
SFLPhoneWindow::initLCD()
{
  mLcd = new SFLLcd(this);
  mLcd->show();
}

void
SFLPhoneWindow::initGUIButtons()
{
  mHangup = new QPushButton(QObject::tr("Hangup"), this, "hangup");
  mHold = new QPushButton(QObject::tr("Hold"), this, "hold");
  mOk = new QPushButton(QObject::tr("Ok"), this, "ok");
  mClear = new QPushButton(QObject::tr("Clear"), this, "clear");
  mMute = new QPushButton(QObject::tr("Mute"), this, "mute");
  mMute->setToggleButton(true);
  mDtmf = new QPushButton(QObject::tr("DTMF"), this, "dtmf");
  mDtmf->setToggleButton(true);
  connect(mKeypad, SIGNAL(isShown(bool)), mDtmf, SLOT(setOn(bool)));
  connect(mDtmf,   SIGNAL(toggled(bool)), this, SLOT(toggleDtmf(bool)));

  mSetup = new QPushButton(QObject::tr("Setup"), this, "setup");
  mTransfer = new QPushButton(QObject::tr("Transfer"), this, "transfer");
  mRedial = new QPushButton(QObject::tr("Redial"), this, "redial");
  mVolume = new VolumeControl(QString(VOLUME_IMAGE),
			      this);
  mVolume->setOrientation(VolumeControl::Vertical);
  mVolume->move(365,91);
  QObject::connect(mVolume, SIGNAL(valueUpdated(int)),
		   this, SIGNAL(volumeUpdated(int)));
  
  mMicVolume = new VolumeControl(QString(VOLUME_IMAGE),
				 this);
  mMicVolume->setOrientation(VolumeControl::Vertical);
  mMicVolume->move(347,91);
  QObject::connect(mVolume, SIGNAL(valueUpdated(int)),
		   this, SIGNAL(micVolumeUpdated(int)));
			      
}

void 
SFLPhoneWindow::initLineButtons()
{
  for(int i = 0; i < NB_PHONELINES; i++) {
    PhoneLineButton *line = new PhoneLineButton(i, this);
    mPhoneLineButtons.push_back(line);
  }
}

void SFLPhoneWindow::initWindowButtons()
{
  mCloseButton = new QPushButton(QObject::tr("Close"), this, "close");
  QObject::connect(mCloseButton, SIGNAL(clicked()),
		   this, SLOT(finish()));
  mMinimizeButton = new QPushButton(QObject::tr("Minimize"), this, "minimize");
  QObject::connect(mMinimizeButton, SIGNAL(clicked()),
		   this, SLOT(showMinimized()));
}

void
SFLPhoneWindow::keyPressEvent(QKeyEvent *e) {
  // Misc. key
  int key = e->key();
  if (e->state() & Qt::ControlButton || key == Qt::Key_Control) {
    emit shortcutPressed(e);
  } else if (key != Qt::Key_Shift && 
      key != Qt::Key_Meta && 
      key != Qt::Key_Alt &&
      key != Qt::Key_Mode_switch
    ) {
    emit keyPressed(Qt::Key(key));
  }
}

void 
SFLPhoneWindow::finish()
{
  emit needToCloseDaemon();
}

void 
SFLPhoneWindow::askReconnect()
{
  QMessageBox::critical(NULL,
			tr("SFLPhone error"),
			tr("We got an error launching sflphone. Check the debug \n"
			   "output with \"[sflphoned]\" for more details. The \n"
			   "application will close."),
			tr("Quit"));
  close();
}

void 
SFLPhoneWindow::askLaunch()
{
  QMessageBox::critical(NULL,
			tr("SFLPhone daemon problem"),
			tr("The SFLPhone daemon couldn't be started. Check \n"
			   "if sflphoned is in your PATH. The application will \n"
			   "close.\n"),
			tr("Quit"));
  close();
}


void
SFLPhoneWindow::showSetup()
{
  mSetupPanel->generate();
  mSetupPanel->show();
}

void
SFLPhoneWindow::hideSetup()
{
  mSetupPanel->hide();
}

void 
SFLPhoneWindow::askResendStatus(QString message)
{
  int ret = QMessageBox::critical(NULL, 
				  tr("SFLPhone status error"),
				  tr("The server returned an error for the lines status.\n"
				     "\n%1\n\n"
				     "Do you want to try to resend this command? If not,\n"
				     "the application will close.").arg(message),
				  QMessageBox::Retry | QMessageBox::Default,
				  QMessageBox::No | QMessageBox::Escape);
  if (ret == QMessageBox::Retry) {
    emit resendStatusAsked();
  }
  else {
    close();
  }
}

void 
SFLPhoneWindow::mousePressEvent(QMouseEvent *e)
{
  mLastPos = e->pos(); // this is relative to the widget
}

void 
SFLPhoneWindow::mouseMoveEvent(QMouseEvent *e)
{
  // Note that moving the windows is very slow
  // 'cause it redraw the screen each time.
  // Usually it doesn't. We could do it by a timer.
  delayedMove(e->globalPos() - mLastPos);
}

void 
SFLPhoneWindow::delayedMove(const QPoint &point) 
{
  mLastWindowPos = point;
}

void
SFLPhoneWindow::delayedPaint()
{
  if(pos() != mLastWindowPos) {
    move(mLastWindowPos);
  }
}

void
SFLPhoneWindow::toggleDtmf(bool toggle) 
{
  if (mKeypad) {
    if (toggle) {
      mKeypad->setDefaultPosition(QPoint(pos().x()+width(), pos().y())); 
    }
    mKeypad->setShown(toggle);
  }
}
