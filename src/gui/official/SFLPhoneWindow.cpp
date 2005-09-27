#include "SFLPhoneWindow.hpp"

#include <QLabel>
#include <QPixmap>
#include <QKeyEvent>
#include <iostream>

#include "globals.h"
#include "PhoneLineButton.hpp"
#include "JPushButton.hpp"

SFLPhoneWindow::SFLPhoneWindow()
  : QMainWindow()
{
  // Initialize the background image
  QLabel *l = new QLabel(this);
  QPixmap main(":/sflphone/images/main.png");
  l->setPixmap(main);
  resize(main.size());
  l->resize(main.size());

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
  
//   QLabel *os = new QLabel(this);
//   QPixmap overscreen(":/images/overscreen.png");
//   os->setPixmap(overscreen);
//   os->resize(overscreen.size());
//   os->move(22,44);

  
  initWindowButtons();
  initLineButtons();
}

SFLPhoneWindow::~SFLPhoneWindow()
{}

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
