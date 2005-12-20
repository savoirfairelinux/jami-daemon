/*
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author: Jerome Oufella (jerome.oufella@savoirfairelinux.com)
 *
 * Portions (c) Jean-Philippe Barrette-LaPierre
 *                (jean-philippe.barrette-lapierre@savoirfairelinux.com)
 * Portions (c) Valentin Heinitz
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

#include <qbitmap.h>
#include <qevent.h>
#include <qimage.h>
#include <qevent.h>

#include "globals.h"

#include "DebugOutput.hpp"
#include "JPushButton.hpp"
#include "TransparentWidget.hpp"

JPushButton::JPushButton(const QString &released,
			 const QString &pressed,
			 QWidget* parent)
  : QLabel(parent)
  , mIsPressed(false)
  , mIsToggling(false)
{
  mImages[0] = transparize(released);
  mImages[1] = transparize(pressed);
  release();
}

JPushButton::~JPushButton()
{}

void
JPushButton::setToggle(bool toggle)
{
  mIsToggling = toggle;
}

QPixmap
JPushButton::transparize(const QString &image)
{
  return TransparentWidget::transparize(image);
}

void 
JPushButton::release()
{
  mIsPressed = false;
  releaseImage();
}

void
JPushButton::press()
{
  mIsPressed = true;
  pressImage();
}

void
JPushButton::releaseImage() 
{
  setPixmap(mImages[0]);
  if(mImages[0].hasAlpha()) {
    setMask(*mImages[0].mask());
  }
  resize(mImages[0].size());
}

void
JPushButton::pressImage() 
{
  setPixmap(mImages[1]);
  if(mImages[1].hasAlpha()) {
    setMask(*mImages[1].mask());
  }
  resize(mImages[1].size());
}

// Mouse button released
void 
JPushButton::mousePressEvent(QMouseEvent *e) 
{
  switch (e->button()) {
  case Qt::LeftButton:
    pressImage();
    break;
    
  default:
    e->ignore();
    break;
  }
}

// Mouse button released
void 
JPushButton::mouseReleaseEvent (QMouseEvent *e) {
  switch (e->button()) {
  case Qt::LeftButton:
    if (this->rect().contains(e->pos())) {
      if(mIsToggling) {
	mIsPressed = !mIsPressed;
	if(mIsPressed) {
	  press();
	}
	else {
	  release();
	}
	emit clicked(mIsPressed);
      }
      else {
	release();
	emit clicked();
      }
    }
    else {
      if(isPressed()) {
	press();
      }
      else {
	release();
      }
    }
    break;
    
  default:
    e->ignore();
    break;
  }
}

void 
JPushButton::mouseMoveEvent(QMouseEvent *e) 
{
  e->accept();
}

