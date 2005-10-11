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

#define LOGO_IMAGE "images/logo_ico.png"
#define BACKGROUND_IMAGE "images/main.png"
#define HANGUP_RELEASED_IMAGE "images/hangup_off.png"
#define HANGUP_PRESSED_IMAGE "images/hangup_on.png"
#define HOLD_RELEASED_IMAGE "images/hold_off.png"
#define HOLD_PRESSED_IMAGE "images/hold_on.png"
#define OK_RELEASED_IMAGE "images/ok_off.png"
#define OK_PRESSED_IMAGE "images/ok_on.png"
#define CLEAR_RELEASED_IMAGE "images/clear_off.png"
#define CLEAR_PRESSED_IMAGE "images/clear_on.png"
#define MUTE_RELEASED_IMAGE "images/mute_off.png"
#define MUTE_PRESSED_IMAGE "images/mute_on.png"
#define VOLUME_IMAGE "images/volume.png"
			    
SFLPhoneWindow::SFLPhoneWindow()
#ifdef QT3_SUPPORT
  : QMainWindow(NULL, Qt::FramelessWindowHint)
#else
    : QMainWindow(NULL, NULL, Qt::WDestructiveClose)
#endif
{
  // Initialize the background image
  mMain = new QLabel(this);
  QPixmap main(JPushButton::transparize(BACKGROUND_IMAGE));
  mMain->setPixmap(main);
  //mMain->move(100,100);
  /*
  if(main.hasAlpha()) {
    setMask(main.mask());
  }
  */

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

  mVolume = new VolumeControl(QString(VOLUME_IMAGE),
			      mMain);
  mVolume->setOrientation(VolumeControl::Vertical);
  mVolume->move(365,91);

  mMicVolume = new VolumeControl(QString(VOLUME_IMAGE),
				 mMain);
  mMicVolume->setOrientation(VolumeControl::Vertical);
  mMicVolume->move(347,91);
			      
}

void 
SFLPhoneWindow::initLineButtons()
{
  int xpos = 21;
  int ypos = 151;
  int offset = 31;
  for(int i = 0; i < NB_PHONELINES; i++) {
    PhoneLineButton *line = new PhoneLineButton(QString("images/l%1_off.png").arg(i + 1),
						QString("images/l%1_on.png").arg(i + 1),
						i,
						mMain);
    line->move(xpos, ypos);
    xpos += offset;
    mPhoneLineButtons.push_back(line);
  }
}

void SFLPhoneWindow::initWindowButtons()
{
  mCloseButton = new JPushButton(":/sflphone/images/close_off.png",
				 ":/sflphone/images/close_on.png",
				 mMain);
  QObject::connect(mCloseButton, SIGNAL(clicked()),
		   this, SLOT(close()));
  mCloseButton->move(374,5);
  mMinimizeButton = new JPushButton(":/sflphone/images/minimize_off.png",
				    ":/sflphone/images/minimize_on.png",
				    mMain);
  QObject::connect(mMinimizeButton, SIGNAL(clicked()),
		   this, SLOT(lower()));
  mMinimizeButton->move(353,5);
}

void
SFLPhoneWindow::keyPressEvent(QKeyEvent *e) {
  // Misc. key	  
  emit keyPressed(Qt::Key(e->key()));
}

void 
SFLPhoneWindow::askReconnect()
{
  int ret = QMessageBox::critical(NULL, 
				  tr("SFLPhone disconnected"),
				  tr("The link between SFLPhone and SFLPhoned is broken.\n"
				     "Do you want to try to reconnect? If not, the application\n"
				     "will close."),
				  QMessageBox::Retry | QMessageBox::Default,
				  QMessageBox::No | QMessageBox::Escape);
  if (ret == QMessageBox::Retry) {
    emit reconnectAsked();
  }
  else {
    close();
  }
}

void 
SFLPhoneWindow::askResendStatus()
{
  int ret = QMessageBox::critical(NULL, 
				  tr("SFLPhone status error"),
				  tr("The server returned an error for the lines status.\n"
				     "Do you want to try to resend this command? If not,\n"
				     "the application will close."),
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
  move(e->globalPos() - mLastPos);
}
