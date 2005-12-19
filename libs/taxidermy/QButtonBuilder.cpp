/*
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author: Jean-Philippe Barrette-LaPierre
 *            (jean-philippe.barrette-lapierre@savoirfairelinux.com)
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dpkg; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <iostream>
#include <qpainter.h>

#include "PaintEventFilter.hpp"
#include "QButtonBuilder.hpp"
#include "qtutils.hpp"

taxidermy::QButtonBuilder::QButtonBuilder()
  : WidgetBuilder("QButton")
    , mButton(NULL)
    , mPosSet(false)
    , mX(0)
    , mY(0)
  , mImages(false)
  , mPaint(false)
{}

void
taxidermy::QButtonBuilder::load(const QMap< QString, QString > &values)
{
  QMap< QString, QString >::ConstIterator xpos = values.find("x");
  QMap< QString, QString >::ConstIterator ypos = values.find("y");
  if(xpos != values.end() && ypos != values.end()) {
    mPosSet = true;
    mX = (*xpos).toInt();
    mY = (*ypos).toInt();
  }

  QMap< QString, QString >::ConstIterator pressed = values.find("pressed");
  QMap< QString, QString >::ConstIterator released = values.find("released");
  QMap< QString, QString >::ConstIterator paint = values.find("paint");
  if(pressed != values.end() && released != values.end()) {
    mImages = true;
    mPressed = qtutils::transparize(*pressed);
    mReleased = qtutils::transparize(*released);
    if(paint != values.end() && (*paint).toInt()) {
      mPaint = true;
    }
  }
}

void
taxidermy::QButtonBuilder::update()
{
  update(mButton);
}

void
taxidermy::QButtonBuilder::update(QWidget *widget)
{  
  setButton(widget);

  if(mButton) {
    if(mPosSet) {
      mButton->move(mX, mY);
    }

    if(mImages) {
      if(mButton->isToggleButton()) {
	QObject::connect(mButton, SIGNAL(toggled(bool)),
			 this, SLOT(toggled(bool)));
      }
      else {
	QObject::connect(mButton, SIGNAL(pressed()),
			 this, SLOT(pressed()));
	QObject::connect(mButton, SIGNAL(released()),
			 this, SLOT(released()));
      }
      if(mPaint) {
	mButton->installEventFilter(new PaintEventFilter(this, SLOT(paintEvent()), mButton));
      }

      if(mButton->isDown()) {
	pressed();
      }
      else {
	released();
      }
    }
  }
}

void
taxidermy::QButtonBuilder::setButton(QWidget *widget) {
  if(widget) {
    mButton = (QButton *)widget;
  }
  else {
    mButton = NULL;
  }
}

void
taxidermy::QButtonBuilder::pressed()
{
  mButton->setPixmap(mPressed);
  if(mPressed.hasAlpha()) {
    mButton->setMask(*mPressed.mask());
  }
  mButton->resize(mPressed.size());
}

void
taxidermy::QButtonBuilder::released()
{
  if(mButton) {
    mButton->setPixmap(mReleased);
    if(mReleased.hasAlpha()) {
      mButton->setMask(*mReleased.mask());
    }
    mButton->resize(mReleased.size());
  }
}

void
taxidermy::QButtonBuilder::toggled(bool on)
{
  if(on) {
    pressed();
  }
  else {
    released();
  }
}

void 
taxidermy::QButtonBuilder::paintEvent() 
{
  const QPixmap *p = mButton->pixmap();
  if (p && !p->isNull()) {
    QPainter painter(mButton);
    painter.drawPixmap(QPoint(0, 0), *p);
  }
}
