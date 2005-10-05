/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
              <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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

#ifndef __VOLUMECONTROL_HPP__
#define __VOLUMECONTROL_HPP__

#include <qlabel.h>
#include "TransparentWidget.hpp"

class VolumeControl : public QLabel
{
  Q_OBJECT

public:
  typedef enum {Vertical = 0, Horizontal} Orientation;

  VolumeControl(const QString &pixmap, 
		QWidget *parent = 0,
		int minValue = 0,
		int maxValue = 100);
  ~VolumeControl(void);

  int getValue()
  {return mValue;}

  int offset();
  int minY();
  int maxY();

signals:
  void valueUpdated(int);

public slots:
  void updateValue();
  void setMin(int value);
  void setMax(int value);
  void setValue(int value);
  void resize();
  void setOrientation(Orientation orientation);

private:
  void updateSlider(int value);
  void mouseMoveEvent(QMouseEvent*);
  void mousePressEvent(QMouseEvent*);


  int mMin;
  int mMax;
  int mValue;

  VolumeControl::Orientation mOrientation;
  QPoint mPos;

  TransparentWidget *mSlider;
  int mMaxPosition;
};

#endif // __VOLUME_CONTROL_H__
