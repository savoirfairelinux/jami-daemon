/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#ifndef __NUMERIC_KEYPAD_HPP__
#define __NUMERIC_KEYPAD_HPP__

#include <map>
#include <qdialog.h>
#include <qpoint.h>

#include "JPushButton.hpp"
#include "TransparentWidget.hpp"

class NumericKeypad : public QDialog
{
  Q_OBJECT
public:
  // Default Constructor and destructor
  NumericKeypad();
  ~NumericKeypad();
  
  JPushButton *mKey0;
  JPushButton *mKey1;
  JPushButton *mKey2;
  JPushButton *mKey3;
  JPushButton *mKey4;
  JPushButton *mKey5;
  JPushButton *mKey6;
  JPushButton *mKey7;
  JPushButton *mKey8;
  JPushButton *mKey9;
  JPushButton *mKeyStar;
  JPushButton *mKeyHash;
  JPushButton *mKeyClose;

public slots:
  void mousePressEvent(QMouseEvent *e);
  void mouseMoveEvent(QMouseEvent *e);
  void keyPressEvent(QKeyEvent *e);
  void keyReleaseEvent(QKeyEvent *e);

  void dtmf0Click();
  void dtmf1Click();
  void dtmf2Click();
  void dtmf3Click();
  void dtmf4Click();
  void dtmf5Click();
  void dtmf6Click();
  void dtmf7Click();
  void dtmf8Click();
  void dtmf9Click();
  void dtmfStarClick();
  void dtmfHashClick();
  void slotHidden();
  
signals:
  void keyPressed(Qt::Key k);
  void isShown(bool);

private:
  QPoint mLastPos;
  std::map< Qt::Key, JPushButton * > mKeys;
};

#endif // __NUMERIC_KEYPAD_H__
