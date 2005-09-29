#include "SFLPhoneWindow.hpp"

#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QKeyEvent>
#include <iostream>

#include "globals.h"
#include "PhoneLineButton.hpp"
#include "JPushButton.hpp"

SFLPhoneWindow::SFLPhoneWindow()
  : QMainWindow(NULL, Qt::FramelessWindowHint)
{
  // Initialize the background image
  QLabel *l = new QLabel(this);
  QPixmap main(":/sflphone/images/main.png");
  l->setPixmap(main);
  resize(main.size());
  l->resize(main.size());

  setWindowIcon(QIcon(QPixmap(":/sflphone/images/logo_ico")));
  setMouseTracking(false);

  mLastPos = pos();
  
//   QLabel *os = new QLabel(this);
//   QPixmap overscreen(":/images/overscreen.png");
//   os->setPixmap(overscreen);
//   os->resize(overscreen.size());
//   os->move(22,44);

  initGUIButtons();
  initWindowButtons();
  initLineButtons();
}

SFLPhoneWindow::~SFLPhoneWindow()
{}

void
SFLPhoneWindow::initGUIButtons()
{
  mHangup = new JPushButton(QPixmap(":/sflphone/images/hangup_off"),
			    QPixmap(":/sflphone/images/hangup_on"),
			    this);
  mHangup->move(225,156);
  
  mHold = new JPushButton(QPixmap(":/sflphone/images/hold_off"),
			  QPixmap(":/sflphone/images/hold_on"),
			  this);
  mHold->move(225,68);
  
  
  mOk = new JPushButton(QPixmap(":/sflphone/images/ok_off"),
			QPixmap(":/sflphone/images/ok_on"),
			this);
  mOk->move(225,182);

  mClear = new JPushButton(QPixmap(":/sflphone/images/clear_off"),
			   QPixmap(":/sflphone/images/clear_on"),
			   this);
  mClear->move(225,130);
}

void 
SFLPhoneWindow::initLineButtons()
{
  int xpos = 21;
  int ypos = 151;
  int offset = 31;
  for(int i = 0; i < NB_PHONELINES; i++) {
    PhoneLineButton *line = new PhoneLineButton(QPixmap(QString(":/sflphone/images/l") +
							QString::number(i + 1) +
							"_off.png"),
						QPixmap(QString(":/sflphone/images/l") +
							QString::number(i + 1) +
							"_on.png"),
						i,
						this);
    line->move(xpos, ypos);
    xpos += offset;
    mPhoneLineButtons.push_back(line);
  }
}

void SFLPhoneWindow::initWindowButtons()
{
  mCloseButton = new JPushButton(QPixmap(":/sflphone/images/close_off.png"),
				 QPixmap(":/sflphone/images/close_on.png"),
				 this);
  QObject::connect(mCloseButton, SIGNAL(clicked()),
		   this, SLOT(close()));
  mCloseButton->move(374,5);
  mMinimizeButton = new JPushButton(QPixmap(":/sflphone/images/minimize_off.png"),
				    QPixmap(":/sflphone/images/minimize_on.png"),
				    this);
  QObject::connect(mMinimizeButton, SIGNAL(clicked()),
		   this, SLOT(lower()));
  mMinimizeButton->move(354,5);
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
    move(e->globalPos() - mLastPos);
}
