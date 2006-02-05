/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include <string>
#include <utility> // for std::make_pair
#include <qapplication.h>
#include <qevent.h>

#include "DebugOutput.hpp"
#include "NumericKeypad.hpp"

#define PIXMAP_KEYPAD_IMAGE QString("dtmf_main.png")
#define DTMF_0_RELEASED_IMAGE QString("dtmf_0_off.png")
#define DTMF_0_PRESSED_IMAGE QString("dtmf_0_on.png")
#define DTMF_1_RELEASED_IMAGE QString("dtmf_1_off.png")
#define DTMF_1_PRESSED_IMAGE QString("dtmf_1_on.png")
#define DTMF_2_RELEASED_IMAGE QString("dtmf_2_off.png")
#define DTMF_2_PRESSED_IMAGE QString("dtmf_2_on.png")
#define DTMF_3_RELEASED_IMAGE QString("dtmf_3_off.png")
#define DTMF_3_PRESSED_IMAGE QString("dtmf_3_on.png")
#define DTMF_4_RELEASED_IMAGE QString("dtmf_4_off.png")
#define DTMF_4_PRESSED_IMAGE QString("dtmf_4_on.png")
#define DTMF_5_RELEASED_IMAGE QString("dtmf_5_off.png")
#define DTMF_5_PRESSED_IMAGE QString("dtmf_5_on.png")
#define DTMF_6_RELEASED_IMAGE QString("dtmf_6_off.png")
#define DTMF_6_PRESSED_IMAGE QString("dtmf_6_on.png")
#define DTMF_7_RELEASED_IMAGE QString("dtmf_7_off.png")
#define DTMF_7_PRESSED_IMAGE QString("dtmf_7_on.png")
#define DTMF_8_RELEASED_IMAGE QString("dtmf_8_off.png")
#define DTMF_8_PRESSED_IMAGE QString("dtmf_8_on.png")
#define DTMF_9_RELEASED_IMAGE QString("dtmf_9_off.png")
#define DTMF_9_PRESSED_IMAGE QString("dtmf_9_on.png")
#define DTMF_STAR_RELEASED_IMAGE QString("dtmf_star_off.png")
#define DTMF_STAR_PRESSED_IMAGE QString("dtmf_star_on.png")
#define DTMF_POUND_RELEASED_IMAGE QString("dtmf_pound_off.png")
#define DTMF_POUND_PRESSED_IMAGE QString("dtmf_pound_on.png")
#define DTMF_CLOSE_RELEASED_IMAGE QString("dtmf_close_off.png")
#define DTMF_CLOSE_PRESSED_IMAGE QString("dtmf_close_on.png")

NumericKeypad::NumericKeypad()
//: TransparentWidget(PIXMAP_KEYPAD_IMAGE, NULL)
  : QDialog(NULL, 
	    "DTMF Keypad", 
	    false,
	    Qt::WStyle_Customize)
{
  TransparentWidget::setPaletteBackgroundPixmap(this, PIXMAP_KEYPAD_IMAGE);
  resize(TransparentWidget::retreive(PIXMAP_KEYPAD_IMAGE).size());
  this->setCaption("DTMF Keypad");
  //setMaximumSize(getSourceImage().width(), getSourceImage().height());
  
  // Buttons initialisation
  mKey0 = new JPushButton(DTMF_0_RELEASED_IMAGE,
			  DTMF_0_PRESSED_IMAGE,
			  this);
  mKey1 = new JPushButton(DTMF_1_RELEASED_IMAGE,
			  DTMF_1_PRESSED_IMAGE,
			  this);
  mKey2 = new JPushButton(DTMF_2_RELEASED_IMAGE,
			  DTMF_2_PRESSED_IMAGE,
			  this);
  mKey3 = new JPushButton(DTMF_3_RELEASED_IMAGE,
			  DTMF_3_PRESSED_IMAGE,
			  this);
  mKey4 = new JPushButton(DTMF_4_RELEASED_IMAGE,
			  DTMF_4_PRESSED_IMAGE,
			  this);
  mKey5 = new JPushButton(DTMF_5_RELEASED_IMAGE,
			  DTMF_5_PRESSED_IMAGE,
			  this);
  mKey6 = new JPushButton(DTMF_6_RELEASED_IMAGE,
			  DTMF_6_PRESSED_IMAGE,
			  this);
  mKey7 = new JPushButton(DTMF_7_RELEASED_IMAGE,
			  DTMF_7_PRESSED_IMAGE,
			  this);
  mKey8 = new JPushButton(DTMF_8_RELEASED_IMAGE,
			  DTMF_8_PRESSED_IMAGE,
			  this);
  mKey9 = new JPushButton(DTMF_9_RELEASED_IMAGE,
			  DTMF_9_PRESSED_IMAGE,
			  this);
  mKeyStar = new JPushButton(DTMF_STAR_RELEASED_IMAGE,
			     DTMF_STAR_PRESSED_IMAGE,
			     this);
  mKeyHash = new JPushButton(DTMF_POUND_RELEASED_IMAGE,
			     DTMF_POUND_PRESSED_IMAGE,
			     this);
  mKeyClose = new JPushButton(DTMF_CLOSE_RELEASED_IMAGE,
			      DTMF_CLOSE_PRESSED_IMAGE,
			      this); 
  connect(mKey0, SIGNAL(clicked()),
	  this, SLOT(dtmf0Click()));
  connect(mKey1, SIGNAL(clicked()),
	  this, SLOT(dtmf1Click()));
  connect(mKey2, SIGNAL(clicked()),
	  this, SLOT(dtmf2Click()));
  connect(mKey3, SIGNAL(clicked()),
	  this, SLOT(dtmf3Click()));
  connect(mKey4, SIGNAL(clicked()),
	  this, SLOT(dtmf4Click()));
  connect(mKey5, SIGNAL(clicked()),
	  this, SLOT(dtmf5Click()));
  connect(mKey6, SIGNAL(clicked()),
	  this, SLOT(dtmf6Click()));
  connect(mKey7, SIGNAL(clicked()),
	  this, SLOT(dtmf7Click()));
  connect(mKey8, SIGNAL(clicked()),
	  this, SLOT(dtmf8Click()));
  connect(mKey9, SIGNAL(clicked()),
	  this, SLOT(dtmf9Click()));
  connect(mKeyStar, SIGNAL(clicked()),
	  this, SLOT(dtmfStarClick()));
  connect(mKeyHash, SIGNAL(clicked()),
	  this, SLOT(dtmfHashClick()));

 
  mKey0->move(58, 157);
  mKey1->move(12, 22);
  mKey2->move(58, 22);
  mKey3->move(104, 22);
  mKey4->move(12, 67);
  mKey5->move(58, 67);
  mKey6->move(104, 67);
  mKey7->move(12, 112);
  mKey8->move(58, 112);
  mKey9->move(104, 112);
  mKeyStar->move(12, 157);
  mKeyHash->move(104, 157);
  mKeyClose->move(141,5);

  mKeys.insert(std::make_pair(Qt::Key_0, mKey0));
  mKeys.insert(std::make_pair(Qt::Key_1, mKey1));
  mKeys.insert(std::make_pair(Qt::Key_2, mKey2));
  mKeys.insert(std::make_pair(Qt::Key_3, mKey3));
  mKeys.insert(std::make_pair(Qt::Key_4, mKey4));
  mKeys.insert(std::make_pair(Qt::Key_5, mKey5));
  mKeys.insert(std::make_pair(Qt::Key_6, mKey6));
  mKeys.insert(std::make_pair(Qt::Key_7, mKey7));
  mKeys.insert(std::make_pair(Qt::Key_8, mKey8));
  mKeys.insert(std::make_pair(Qt::Key_9, mKey9));
  mKeys.insert(std::make_pair(Qt::Key_Asterisk, mKeyStar));
  mKeys.insert(std::make_pair(Qt::Key_NumberSign, mKeyHash));

  connect(mKeyClose, SIGNAL(clicked()),
	  this, SLOT(hide()));
  connect(mKeyClose, SIGNAL(clicked()),
	  this, SLOT(slotHidden()));
}

NumericKeypad::~NumericKeypad() 
{}

void
NumericKeypad::keyReleaseEvent (QKeyEvent* e) {  
  std::map< Qt::Key, JPushButton * >::iterator pos = mKeys.find(Qt::Key(e->key()));
  if(pos != mKeys.end()) {
    QMouseEvent me(QEvent::MouseButtonRelease,
		   QPoint(0,0),
		   Qt::LeftButton,
		   Qt::LeftButton);
    QApplication::sendEvent(pos->second, 
			    &me);
  }
}

void
NumericKeypad::keyPressEvent (QKeyEvent* e) {  
  //QApplication::sendEvent(QApplication::mainWindow, e);
  // TODO: Key appears pressed when done.
  std::map< Qt::Key, JPushButton * >::iterator pos = mKeys.find(Qt::Key(e->key()));
  if(pos != mKeys.end()) {
    QMouseEvent me(QEvent::MouseButtonPress,
		   QPoint(0,0),
		   Qt::LeftButton,
		   Qt::LeftButton);
    QApplication::sendEvent(pos->second, 
			    &me);
  }
  else {
    emit keyPressed(Qt::Key(e->key()));
  }
}


void 
NumericKeypad::mousePressEvent(QMouseEvent *e)
{
  mLastPos = e->pos();
}

void 
NumericKeypad::mouseMoveEvent(QMouseEvent *e)
{
  // Note that moving the windows is very slow
  // 'cause it redraw the screen each time.
  // Usually it doesn't. We could do it by a timer.
  move(e->globalPos() - mLastPos);
}

void
NumericKeypad::dtmf0Click()
{
  emit keyPressed(Qt::Key_0);
}

void
NumericKeypad::dtmf1Click()
{emit keyPressed(Qt::Key_1);}

void
NumericKeypad::dtmf2Click()
{emit keyPressed(Qt::Key_2);}

void
NumericKeypad::dtmf3Click()
{emit keyPressed(Qt::Key_3);}

void
NumericKeypad::dtmf4Click()
{emit keyPressed(Qt::Key_4);}

void
NumericKeypad::dtmf5Click()
{emit keyPressed(Qt::Key_5);}

void
NumericKeypad::dtmf6Click()
{emit keyPressed(Qt::Key_6);}

void
NumericKeypad::dtmf7Click()
{emit keyPressed(Qt::Key_7);}

void
NumericKeypad::dtmf8Click()
{emit keyPressed(Qt::Key_8);}

void
NumericKeypad::dtmf9Click()
{emit keyPressed(Qt::Key_9);}

void
NumericKeypad::dtmfStarClick()
{emit keyPressed(Qt::Key_Asterisk);}

void
NumericKeypad::dtmfHashClick()
{emit keyPressed(Qt::Key_NumberSign);}

void 
NumericKeypad::slotHidden() 
{
  emit isShown(false);
}
