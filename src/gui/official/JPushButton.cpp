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

#include <QEvent>
#include <QMouseEvent>

#include "globals.h"

#include "JPushButton.hpp"

JPushButton::JPushButton(const QPixmap &released,
			 const QPixmap &pressed,
			 QWidget* parent, 
			 Qt::WFlags flags)
  : QLabel(parent, flags) 
{
  mImages[0] = released;
  mImages[1] = pressed;
  release();
}

JPushButton::~JPushButton()
{}

void
JPushButton::release() 
{
  mIsPressed = false;
  resize(mImages[0].size());
  setPixmap(mImages[0]);
}

void
JPushButton::press() 
{
  mIsPressed = true;
  resize(mImages[1].size());
  setPixmap(mImages[1]);
}

// Mouse button released
void 
JPushButton::mousePressEvent(QMouseEvent *e) 
{
  switch (e->button()) {
  case Qt::LeftButton:
    press();
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
    release();
    // Emulate the left mouse click
    if (this->rect().contains(e->pos())) {
      emit clicked();
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

