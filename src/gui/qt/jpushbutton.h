/*
 * Copyright (C) 2004 Savoir-Faire Linux inc.
 * Author: Jerome Oufella (jerome.oufella@savoirfairelinux.com)
 *
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
#include <qbitmap.h>
#include <qevent.h>
#include <qimage.h>
#include <qobject.h>
#include <qlabel.h>
#include <qwidget.h>
#include <qstring.h>

#define	PRESS_PREFIX	"_on"
#define REL_PREFIX		"_off"

class QtGUIMainWindow;

class JPushButton : public QLabel {
Q_OBJECT

public:
	JPushButton (QWidget*, const char*, const char*);
	~JPushButton (void);

private:
	QtGUIMainWindow*	guiWidget;
	
	void iAmPressed (void);
	void iAmReleased (void);
	QImage*		btnImg[2];
	QBitmap		mask[2];
	void		loadPixmaps (const char*);

	// This function was derived from QImage::createHeuristicMask()
	int MyCreateHeuristicMask (const  QImage &, QImage &, long = -1 );


protected:
	void		mousePressEvent		(QMouseEvent *);
	void		mouseReleaseEvent	(QMouseEvent *);
	void		mouseMoveEvent		(QMouseEvent *);

signals:
	void		clicked				(void);
};

#endif	// defined(__J_PUSH_BUTTON_H__)
