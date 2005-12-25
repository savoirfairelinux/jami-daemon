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

#include "qtutils.hpp"

#include <qbitmap.h>
#include <qimage.h>
#include <qmime.h>


void
taxidermy::qtutils::addFilePath(const QString &path)
{
  QMimeSourceFactory *factory = QMimeSourceFactory::defaultFactory();
  factory->addFilePath(path);
}

QPixmap 
taxidermy::qtutils::transparize(const QString &image)
{
#ifdef QT3_SUPPORT
  QPixmap p(retreive(image));
  if (!p.mask()) {
    if (p.hasAlphaChannel()) {
      p.setMask(p.alphaChannel());
    } 
    else {
      p.setMask(p.createHeuristicMask());
    }
  }
#else
  QImage img(QImage::fromMimeSource(image));
  QPixmap p;
  p.convertFromImage(img);
  
  
    QBitmap bm;
    if (img.hasAlphaBuffer()) {
      bm = img.createAlphaMask();
    } 
    else {
      bm = img.createHeuristicMask();
    }
    p.setMask(bm);
#endif
  return p;
}

