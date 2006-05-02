/*
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
#include <qpainter.h>


PhoneLineButton::PhoneLineButton(unsigned int line,
				 QWidget *parent)
  : QPushButton(parent)
  , mLine(line)
  , mFace(0)
{
  setName(QObject::tr("line%1").arg(line));
  setToggleButton(true);
  mTimer = new QTimer(this);
  connect(mTimer, SIGNAL(timeout()),
	  this, SLOT(swap()));
  connect(this, SIGNAL(clicked()),
	  this, SLOT(sendClicked()));
}

void
PhoneLineButton::setToolTip(QString tip)
{
  QToolTip::add(this, tip);
}

void
PhoneLineButton::swap()
{
  toggle();
}

void
PhoneLineButton::clearToolTip()
{
  QToolTip::remove(this);
}

void
PhoneLineButton::suspend()
{
  setDown(false);
  mTimer->start(500);
}

void
PhoneLineButton::sendClicked()
{
  if(isOn()) {
    mTimer->stop();
    emit selected(mLine);
  }
  else {
    emit unselected(mLine);
  }
}

