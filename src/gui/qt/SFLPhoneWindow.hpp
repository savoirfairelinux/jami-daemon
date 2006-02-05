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

#include <qlabel.h>
#include <qmainwindow.h>
#include <qobject.h>
#include <qpoint.h>
#include <qpushbutton.h>
#include <qtimer.h>
#include <list>

#include "ConfigurationPanel.h"
#include "PhoneLineManager.hpp"

class JPushButton;
class PhoneLineButton;
class SFLLcd;
class VolumeControl;

class SFLPhoneWindow : public QMainWindow
{
  Q_OBJECT;

  friend class SFLPhoneApp;
  
public:
  SFLPhoneWindow();
  ~SFLPhoneWindow();

private:
  void initLCD();
  void initGUIButtons();
  void initLineButtons();
  void initWindowButtons();

signals:
  void keyPressed(Qt::Key);
  void shortcutPressed(QKeyEvent *e);
  void launchAsked();
  void reconnectAsked();
  void resendStatusAsked();
  void volumeUpdated(int);
  void micVolumeUpdated(int);
  void needToCloseDaemon();
  void ringtonesUpdated();
  void audioDevicesUpdated();
  void codecsUpdated();
  void needRegister();
  void registerFailed(QString);
  void registerSucceed(QString);


public slots:
  void delayedMove(const QPoint &point);
  void delayedPaint();

  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);

  /**
   * This function will prompt a message box, to ask
   * if the user want to reconnect to sflphoned.
   */
  void askReconnect();

  /**
   * This function will prompt a message box, to ask
   * if the user want to launch sflphoned.
   */
  void askLaunch();

  /**
   * This function will ask if you want to close 
   * sflphoned.
   */
  void finish();

  /**
   * This function will prompt a message box, to ask
   * if the user want to resend the getcallstatus request.
   */
  void askResendStatus(QString);

  void showSetup();
  void hideSetup();

protected:
  void keyPressEvent(QKeyEvent *e);

private:
  std::list< PhoneLineButton * > mPhoneLineButtons;

  QPushButton *mCloseButton;
  QPushButton *mMinimizeButton;

  QPushButton *mHangup;
  QPushButton *mHold;
  QPushButton *mOk;
  QPushButton *mClear;
  QPushButton *mMute;
  QPushButton *mDtmf;
  QPushButton *mSetup;
  QPushButton *mTransfer;
  QPushButton *mRedial;
  
  VolumeControl *mVolume;
  VolumeControl *mMicVolume;

  SFLLcd *mLcd;
  QLabel *mMain;

  QPoint mLastPos;
  QPoint mLastWindowPos;
  QTimer *mPaintTimer;

  ConfigurationPanel *mSetupPanel;
};
