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

#include "globals.h" 

#include "PhoneLineButton.hpp"

#include <qevent.h>
#include <qtimer.h>
#include <qtooltip.h>


PhoneLineButton::PhoneLineButton(const QString &released, 
				 const QString &pressed,
				 unsigned int line,
				 QWidget *parent)
  : JPushButton(released, pressed, parent)
  , mLine(line)
  , mFace(0)
{
  mTimer = new QTimer(this);
  connect(mTimer, SIGNAL(timeout()),
	  this, SLOT(swap()));
}

void
PhoneLineButton::setToolTip(QString tip)
{
  QToolTip::add(this, tip);
}

void
PhoneLineButton::clearToolTip()
{
  QToolTip::remove(this);
}

void
PhoneLineButton::suspend()
{
  if(isPressed()) {
    mFace = 1;
  }
  else {
    mFace = 0;
  }
  swap();
  mTimer->start(500);
}

void
PhoneLineButton::swap()
{
  mFace = (mFace + 1) % 2;
  resize(mImages[mFace].size());
  setPixmap(mImages[mFace]);
}

void 
PhoneLineButton::press()
{
  mTimer->stop();
  JPushButton::press();
}

void 
PhoneLineButton::release()
{
  mTimer->stop();
  JPushButton::release();
}

void
PhoneLineButton::mouseReleaseEvent (QMouseEvent *e)
{
  switch (e->button()) {
  case Qt::LeftButton:
    // Emulate the left mouse click
    if (this->rect().contains(e->pos())) {
      emit clicked(mLine);
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
