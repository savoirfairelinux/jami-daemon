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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef __PHONELINEBUTTON_HPP__
#define __PHONELINEBUTTON_HPP__

#include <qlabel.h>
#include <qobject.h>
#include <qpixmap.h>

#include "JPushButton.hpp"

class QTimer;


/**
 * This class Emulate a PushButton but takes two
 * images to display its state.
 */
class PhoneLineButton : public JPushButton
{
  Q_OBJECT
  
public:
  PhoneLineButton(const QString &released, 
		  const QString &pressed,
		  unsigned int line,
		  QWidget *parent);

  virtual ~PhoneLineButton(){}
  
signals:
  void clicked(unsigned int);
  
public slots:
  virtual void suspend();
  virtual void press();
  virtual void release();
  virtual void setToolTip(QString);
  virtual void clearToolTip();
  
private slots:
  void swap();
  
protected:
  void mouseReleaseEvent(QMouseEvent *);

private:
  unsigned int mLine;
  QTimer *mTimer;
  unsigned int mFace;
  
};

#endif	// defined(__J_PUSH_BUTTON_H__)
