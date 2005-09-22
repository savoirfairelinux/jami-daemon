/*
 * Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 * Author: Jerome Oufella (jerome.oufella@savoirfairelinux.com)
 *
 * Portions (c) Jean-Philippe Barrette-LaPierre
 *                (jean-philippe.barrette-lapierre@savoirfairelinux.com)
 * Portions (c) Valentin Heinitz
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

#ifndef __J_PUSH_BUTTON_H__
#define __J_PUSH_BUTTON_H__

#include <QLabel>
#include <QPixmap>

/**
 * This class Emulate a PushButton but takes two
 * images to display its state.
 */
class JPushButton : public QLabel {
  Q_OBJECT
    
public:
  JPushButton(const QPixmap &released, 
	      const QPixmap &pressed,
	      QWidget *parent, 
	      Qt::WFlags flags = 0);
  ~JPushButton();
  
  /**
   * This function will switch the button
   */
  void press();
  void release();

private:
  QPixmap mImages[2];
  
protected:
  void mousePressEvent(QMouseEvent *);
  void mouseReleaseEvent(QMouseEvent *);
  void mouseMoveEvent(QMouseEvent *);
  
signals:
  void clicked(void);

};

#endif	// defined(__J_PUSH_BUTTON_H__)
