/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#include <qevent.h>
#include <iostream>

#include "VolumeControl.hpp"
#include "TransparentWidget.hpp"

VolumeControl::VolumeControl (const QString &pixname,
			      QWidget *parent, 
			      int minValue,
			      int maxValue) 
  : TransparentWidget(parent) 
  , mMin(minValue)
  , mMax(maxValue)
  , mValue(minValue)
  , mOrientation(VolumeControl::Horizontal)
  , mSlider(new TransparentWidget(pixname, this))
  , mMaxPosition(100)
{
  resize();
}
	      
VolumeControl::~VolumeControl()
{}

void
VolumeControl::resize()
{
  if(mOrientation == VolumeControl::Horizontal) {
    QLabel::resize(QSize(mSlider->size().width(), 
			 mMaxPosition + mSlider->size().height()));
  }
  else {
    QLabel::resize(QSize(mMaxPosition + mSlider->size().width(), 
			 mSlider->size().height()));
  }
}

void 
VolumeControl::setOrientation(VolumeControl::Orientation orientation)
{
  mOrientation = orientation;
}

void 
VolumeControl::setMax(int value)
{
  if(value >= mMin) {
    mMax = value;
  }
}

void 
VolumeControl::setMin(int value)
{
  if(value <= mMax) {
    mMin = value;
  }
}

void 
VolumeControl::setValue(int value) 
{
  if(value <= mMax && value >= mMin) {
    mValue = value;
  }
}


void
VolumeControl::mouseMoveEvent (QMouseEvent *e) {
  if (mOrientation == VolumeControl::Vertical) {
    // If the slider for the volume is vertical	
    int yoffset = e->y() - mPos.y();
    std::cout << "yoffset: " << yoffset << std::endl;
    if(yoffset < 0) {
      yoffset = 0;
    }

    if(yoffset > mMaxPosition) {
      yoffset = mMaxPosition;
    } 
      
    std::cout << "new yoffset: " << yoffset << std::endl << std::endl;

    mSlider->move(mSlider->x(), yoffset);
  }
  else {
    mSlider->move(e->y() - mPos.x(), mSlider->y());
  }
}

void
VolumeControl::mousePressEvent (QMouseEvent *e) 
{
  mPos = e->pos();
}

// EOF
