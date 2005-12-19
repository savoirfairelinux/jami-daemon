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

#ifndef __TAXIDERMY_QBUTTON_BUILDER_HPP__
#define __TAXIDERMY_QBUTTON_BUILDER_HPP__

#include <qbutton.h>
#include <qpixmap.h>
#include <qwidget.h>

#include "WidgetBuilder.hpp"

namespace taxidermy 
{
  class QButtonBuilder : public WidgetBuilder
  {
    Q_OBJECT;

  private:
    QButton *mButton;

    //Position
    bool mPosSet;
    int mX;
    int mY;

    //Images
    bool mImages;
    bool mPaint;
    QPixmap mPressed;
    QPixmap mReleased;

    virtual void setButton(QWidget *widget);

 public slots:
    void pressed();
    void released();
    void toggled(bool);
    void paintEvent();

  public:
    QButtonBuilder();

    virtual void load(const QMap< QString, QString > &entries);
    virtual void update();
    virtual void update(QWidget *widget);
  };
};

#endif
