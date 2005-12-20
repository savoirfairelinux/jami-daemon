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
  connect(this, SIGNAL(ringtonesUpdated()),
	  mSetupPanel, SLOT(updateRingtones()));
  connect(this, SIGNAL(audioDevicesUpdated()),
	  mSetupPanel, SLOT(updateAudioDevices()));
  connect(this, SIGNAL(codecsUpdated()),
	  mSetupPanel, SLOT(updateCodecs()));
  connect(mSetupPanel, SIGNAL(needRegister()),
	  this, SIGNAL(needRegister()));

  // Initialize the background image
  mMain = new QLabel(this);
  QPixmap main(JPushButton::transparize(BACKGROUND_IMAGE));
  mMain->setPixmap(main);
  if(main.hasAlpha()) {
    setMask(*main.mask());
  }

  mPaintTimer = new QTimer(this);
  connect(mPaintTimer, SIGNAL(timeout()),
	  this, SLOT(delayedPaint()));
  mPaintTimer->start(50);
  

  resize(main.size());
  mMain->resize(main.size());

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
  mLcd = new SFLLcd(mMain);
  mLcd->show();
}

void
SFLPhoneWindow::initGUIButtons()
{
  mHangup = new QPushButton(QObject::tr("Hangup"), mMain, "hangup");
  mHold = new QPushButton(QObject::tr("Hold"), mMain, "hold");
  mOk = new QPushButton(QObject::tr("Ok"), mMain, "ok");
  mClear = new QPushButton(QObject::tr("Clear"), mMain, "clear");
  mMute = new QPushButton(QObject::tr("Mute"), mMain, "mute");
  mMute->setToggleButton(true);
  mDtmf = new QPushButton(QObject::tr("DTMF"), mMain, "dtmf");
  mDtmf->setToggleButton(true);
  mSetup = new QPushButton(QObject::tr("Setup"), mMain, "setup");
  mTransfer = new QPushButton(QObject::tr("Transfer"), mMain, "transfer");
  mRedial = new QPushButton(QObject::tr("Redial"), mMain, "redial");
  mVolume = new VolumeControl(QString(VOLUME_IMAGE),
			      mMain);
  mVolume->setOrientation(VolumeControl::Vertical);
  mVolume->move(365,91);
  QObject::connect(mVolume, SIGNAL(valueUpdated(int)),
		   this, SIGNAL(volumeUpdated(int)));
  
  mMicVolume = new VolumeControl(QString(VOLUME_IMAGE),
				 mMain);
  mMicVolume->setOrientation(VolumeControl::Vertical);
  mMicVolume->move(347,91);
  QObject::connect(mVolume, SIGNAL(valueUpdated(int)),
		   this, SIGNAL(micVolumeUpdated(int)));
			      
}

void 
SFLPhoneWindow::initLineButtons()
{
  for(int i = 0; i < NB_PHONELINES; i++) {
    PhoneLineButton *line = new PhoneLineButton(i, mMain);
    mPhoneLineButtons.push_back(line);
  }
}

void SFLPhoneWindow::initWindowButtons()
{
  mCloseButton = new QPushButton(QObject::tr("Close"), mMain, "close");
  QObject::connect(mCloseButton, SIGNAL(clicked()),
		   this, SLOT(finish()));
  mMinimizeButton = new QPushButton(QObject::tr("Minimize"), mMain, "minimize");
  QObject::connect(mMinimizeButton, SIGNAL(clicked()),
		   this, SLOT(showMinimized()));
}

void
SFLPhoneWindow::keyPressEvent(QKeyEvent *e) {
  // Misc. key	  
  if (e->state() & Qt::ControlButton || e->key() == Qt::Key_Control) {
    emit shortcutPressed(e);
  } else {
    emit keyPressed(Qt::Key(e->key()));
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
  mLastPos = e->pos();
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
