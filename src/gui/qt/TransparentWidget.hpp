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

#ifndef __TRANSPARENT_WIDGET_HPP__
#define __TRANSPARENT_WIDGET_HPP__

#include <qbitmap.h>
#include <qlabel.h>
#include <qpixmap.h>
#include <qimage.h>

/**
 * This class Emulate a PushButton but takes two
 * images to display its state.
 */
class TransparentWidget : public QLabel 
{
  Q_OBJECT
    
public:
  TransparentWidget(const QString &pixmap, 
		    QWidget *parent);
  TransparentWidget(QWidget *parent);
  ~TransparentWidget();

  static QPixmap retreive(const QString &size);
  static QPixmap transparize(const QSize &size);
  static QPixmap transparize(const QString &image);
  static void setPaletteBackgroundPixmap(QWidget *w, const QString &pixmap);

  /**
   * This function will update the mask of the widget
   * to the QPixmap mask.
   */
  static void updateMask(QWidget *w, QPixmap image);


  bool hasAlpha()
  {return mImage.hasAlpha();}

  QBitmap mask() const
  {return *mImage.mask();}
  
private:  
  QPixmap mImage;

};

#endif
