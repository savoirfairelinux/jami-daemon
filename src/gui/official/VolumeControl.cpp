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
  : QLabel(parent) 
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
    QWidget::resize(QSize(mSlider->size().width(), 
			 mMaxPosition + mSlider->size().height()));
  }
  else {
    QWidget::resize(QSize(mMaxPosition + mSlider->size().width(), 
			 mSlider->size().height()));
  }
  QPixmap q(TransparentWidget::transparize(QString(":/sflphone/images/slider")));
  std::cout << "mask isNull: " << q.mask().isNull() << std::endl;
  std::cout << "isNull: " << q.isNull() << std::endl;
  std::cout << q.size().width() << "," << q.size().height() << std::endl;
  setPixmap(q);
  if(q.hasAlpha()) {
    std::cout << "has alpha" << std::endl;
    setMask(q.mask());
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
  if(value != mValue) {
    if(value <= mMax && value >= mMin) {
      mValue = value;
      updateSlider(value);
      emit valueUpdated(mValue);
    }
  }
}


void
VolumeControl::mouseMoveEvent (QMouseEvent *e) {
  if (mOrientation == VolumeControl::Vertical) {
    // If the slider for the volume is vertical	
    int newpos = mSlider->y() + e->globalY() - mPos.y();
      
    mPos = e->globalPos();
    if(newpos < 0) {
      mPos.setY(mPos.y() - newpos);
      newpos = 0;
    }

    if(newpos > mMaxPosition) {
      mPos.setY(mPos.y() - (newpos - mMaxPosition));
      newpos = mMaxPosition;
    } 

    mSlider->move(mSlider->x(), newpos);
    updateValue();
  }
  else {
    mSlider->move(e->y() - mPos.x(), mSlider->y());
  }

}

void
VolumeControl::updateValue()
{
  int value = (int)((float)offset() / mMaxPosition * (mMax - mMin));
  mValue = value;
  emit valueUpdated(mValue);
}


void
VolumeControl::updateSlider(int value)
{
  if(mOrientation == VolumeControl::Vertical) {
    std::cout <<  "Move again to : " << 
      value / (mMax - mMin) * mMaxPosition << 
      std::endl << std::endl;
    mSlider->move(mSlider->x(), value / (mMax - mMin) * mMaxPosition);
  }
  else {
    mSlider->move(value / (mMax - mMin) * mMaxPosition, mSlider->y());
  }
}

int
VolumeControl::offset()
{
  if(mOrientation == VolumeControl::Vertical) {
    return mSlider->y();
  }
  else {
    return mSlider->x();
  }
}

void
VolumeControl::mousePressEvent (QMouseEvent *e) 
{
  mPos = e->globalPos();
}

// EOF
