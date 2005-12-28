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
#include "QWidgetBuilder.hpp"
#include "qtutils.hpp"

taxidermy::QWidgetBuilder::QWidgetBuilder()
  : WidgetBuilder("QWidget")
  , mWidget(NULL)
  , mPosSet(false)
{}

void
taxidermy::QWidgetBuilder::load(const QMap< QString, QString > &values)
{
  QMap< QString, QString >::ConstIterator xpos = values.find("x");
  QMap< QString, QString >::ConstIterator ypos = values.find("y");
  if(xpos != values.end() && ypos != values.end()) {
    mPosSet = true;
    mX = (*xpos).toInt();
    mY = (*ypos).toInt();
  }

  QMap< QString, QString >::ConstIterator background = values.find("background");
  if(background != values.end()) {
    mBackground = qtutils::transparize(*background);
  }
}

void
taxidermy::QWidgetBuilder::update()
{
  update(mWidget);
}

void
taxidermy::QWidgetBuilder::update(QWidget *widget)
{  
  mWidget = widget;

  if(mWidget) {
    if(mPosSet) {
      mWidget->move(mX, mY);
    }

    if(!mBackground.isNull()) {
      mWidget->setPaletteBackgroundPixmap(mBackground);
      if(mBackground.hasAlpha()) {
	mWidget->setMask(*mBackground.mask());
      }
      mWidget->resize(mBackground.size());
    }
  }
}

