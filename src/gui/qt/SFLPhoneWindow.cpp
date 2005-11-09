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
#define HANGUP_RELEASED_IMAGE "hangup_off.png"
#define HANGUP_PRESSED_IMAGE "hangup_on.png"
#define HOLD_RELEASED_IMAGE "hold_off.png"
#define HOLD_PRESSED_IMAGE "hold_on.png"
#define OK_RELEASED_IMAGE "ok_off.png"
#define OK_PRESSED_IMAGE "ok_on.png"
#define CLEAR_RELEASED_IMAGE "clear_off.png"
#define CLEAR_PRESSED_IMAGE "clear_on.png"
#define MUTE_RELEASED_IMAGE "mute_off.png"
#define MUTE_PRESSED_IMAGE "mute_on.png"
#define DTMF_RELEASED_IMAGE "dtmf_off.png"
#define DTMF_PRESSED_IMAGE "dtmf_on.png"
#define VOLUME_IMAGE "volume.png"
#define CLOSE_RELEASED_IMAGE "close_off.png"
#define CLOSE_PRESSED_IMAGE "close_on.png"
#define MINIMIZE_RELEASED_IMAGE "minimize_off.png"
#define MINIMIZE_PRESSED_IMAGE "minimize_on.png"
#define SETUP_RELEASED_IMAGE "setup_off.png"
#define SETUP_PRESSED_IMAGE "setup_on.png"
#define TRANSFERT_RELEASE_IMAGE "transfer_off.png"
#define TRANSFERT_PRESSED_IMAGE "transfer_on.png"

			    
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
  mHangup = new JPushButton(QString(HANGUP_RELEASED_IMAGE),
			    QString(HANGUP_PRESSED_IMAGE),
			    mMain);
  mHangup->move(225,156);
  
  mHold = new JPushButton(QString(HOLD_RELEASED_IMAGE),
			  QString(HOLD_PRESSED_IMAGE),
						  mMain);
  mHold->move(225,68);
  
  
  mOk = new JPushButton(QString(OK_RELEASED_IMAGE),
			QString(OK_PRESSED_IMAGE),
			mMain);
  mOk->move(225,182);

  mClear = new JPushButton(QString(CLEAR_RELEASED_IMAGE),
			   QString(CLEAR_PRESSED_IMAGE),
			   mMain);
  mClear->move(225,130);

  mMute = new JPushButton(QString(MUTE_RELEASED_IMAGE),
			  QString(MUTE_PRESSED_IMAGE),
			   mMain);
  mMute->move(225,94);
  mMute->setToggle(true);

  mDtmf = new JPushButton(QString(DTMF_RELEASED_IMAGE),
			  QString(DTMF_PRESSED_IMAGE),
			  mMain);
  mDtmf->move(20,181);
  mDtmf->setToggle(true);

  mSetup = new JPushButton(QString(SETUP_RELEASED_IMAGE),
			   QString(SETUP_PRESSED_IMAGE),
			   mMain);
  //mSetup->move(225,42);
  mSetup->move(318,68);

  mTransfer = new JPushButton(QString(TRANSFERT_RELEASE_IMAGE),
			      QString(TRANSFERT_PRESSED_IMAGE),
			      mMain);
  mTransfer->move(225,42);
  //mTransfer->hide();

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
  int xpos = 21;
  int ypos = 151;
  int offset = 31;
  for(int i = 0; i < NB_PHONELINES; i++) {
    PhoneLineButton *line = new PhoneLineButton(QString("l%1_off.png").arg(i + 1),
						QString("l%1_on.png").arg(i + 1),
						i,
						mMain);
    line->move(xpos, ypos);
    xpos += offset;
    mPhoneLineButtons.push_back(line);
  }
}

void SFLPhoneWindow::initWindowButtons()
{
  mCloseButton = new JPushButton(CLOSE_RELEASED_IMAGE,
				 CLOSE_PRESSED_IMAGE,
				 mMain);
  QObject::connect(mCloseButton, SIGNAL(clicked()),
		   this, SLOT(finish()));
  mCloseButton->move(374,5);
  mMinimizeButton = new JPushButton(MINIMIZE_RELEASED_IMAGE,
				    MINIMIZE_PRESSED_IMAGE,
				    mMain);
  QObject::connect(mMinimizeButton, SIGNAL(clicked()),
		   this, SLOT(showMinimized()));
  mMinimizeButton->move(353,5);
}

void
SFLPhoneWindow::keyPressEvent(QKeyEvent *e) {
  // Misc. key	  
  emit keyPressed(Qt::Key(e->key()));
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
