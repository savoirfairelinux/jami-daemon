/*
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author: Jean-Philippe Barrette-LaPierre
 *           (jean-philippe.barrette-lapierre@savoirfairelinux.com)
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

#include <QBitmap>
#include <QColor>
#include <iostream>

#include "TransparentWidget.hpp"


TransparentWidget::TransparentWidget(const QString &pixmap,
			 QWidget* parent, 
			 Qt::WFlags flags)
  : QLabel(parent, flags) 
{
  mImage = transparize(pixmap);
  setPixmap(mImage);
  if(mImage.hasAlpha()) {
    setMask(mImage.mask());
  }
  resize(mImage.size());
}

TransparentWidget::TransparentWidget(QWidget* parent, 
				     Qt::WFlags flags)
  : QLabel(parent, flags) 
{}


QPixmap
TransparentWidget::transparize(const QSize &size)
{

  QImage image(size, QImage::Format_RGB32);
  QColor c(12,32,35,123);
  image.fill(c.rgb());

  QPixmap p(QPixmap::fromImage(image));
  p.setMask(p.createHeuristicMask());
  //p.setMask(p.alphaChannel());

  return p;
}

TransparentWidget::~TransparentWidget()
{}

QPixmap
TransparentWidget::transparize(const QString &image)
{
  QPixmap p(image);
  
  if (!p.mask()) {
    if (p.hasAlphaChannel()) {
      p.setMask(p.alphaChannel());
    } 
    else {
      p.setMask(p.createHeuristicMask());
    }
  }

  return p;
}


