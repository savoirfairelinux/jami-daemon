/**
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __SFLLCD_HPP__
#define __SFLLCD_HPP__

#include <qlabel.h>
#include <qobject.h>
#include <qpixmap.h>
#include <qdatetime.h>
#include <qtimer.h>

class SFLLcd : public QLabel
{
  Q_OBJECT
  
public:
  SFLLcd(QWidget *parent = NULL);

  bool textIsTooBig(const QString &text);

public slots:
  virtual void paintEvent(QPaintEvent *event);
  QString getTimeStatus();

  void setGlobalStatus(QString global);
  void setUnselectedLineStatus(QString line);
  void setLineStatus(QString line);
  void setAction(QString line);
  void setBufferStatus(QString line);
  void setLineTimer(QTime time);
  void clearLineTimer();

  void updateText();
  void updateGlobalText();
  QString extractVisibleText(const QString &text, int &pos);
  
private:
  QPixmap mScreen;
  QPixmap mOverscreen;
  
  QString mGlobalStatus;
  QString mUnselectedLineStatus;
  QString mLineStatus;
  QString mBufferStatus;
  QString mAction;
  int mGlobalStatusPos;
  int mUnselectedLineStatusPos;
  int mLineStatusPos;
  int mBufferStatusPos;
  int mActionPos;

  bool mIsTimed;
  QTime mTime;
  QTimer *mTimer;
  QTimer *mUnselectedLineTimer;

  QFont mFont;
};

#endif
