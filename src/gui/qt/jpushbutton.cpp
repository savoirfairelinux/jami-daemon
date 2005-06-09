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

#include <qcursor.h>
#include <qwidget.h>
#include <qlabel.h>
#include <qimage.h>
#include <qbitmap.h>
#include <qevent.h>

#include "jpushbutton.h"
#include "../../skin.h"
#include "qtGUImainwindow.h"

// This is the default constructor, it must be called with pixname being
// the base name for the picture used as button pixmap.
JPushButton::JPushButton (QWidget* parent, const char* name,
	const char* pixname) : QLabel (parent, name) {

	guiWidget = (QtGUIMainWindow*)parent;

	// Load pictures
	this->loadPixmaps(pixname);

	// Create transparency bitmasks
	QImage tmpImg[2];
	MyCreateHeuristicMask (*(this->btnImg[0]), tmpImg[0]);
	mask[0] = tmpImg[0];

	MyCreateHeuristicMask (*(this->btnImg[1]), tmpImg[1]);
	mask[1] = tmpImg[1];

	// Resize ourself
	resize (this->btnImg[0]->width(), this->btnImg[0]->height() );
	
	// Set default pixmap (released)
	setMask (mask[0]);
	setPixmap (*(this->btnImg[0]));

	// Default cursor is pointing hand
	setCursor (QCursor (Qt::PointingHandCursor));

	setFocusPolicy(QWidget::NoFocus);
	show();
}

// Delete allocated items
JPushButton::~JPushButton (void) {
	delete this->btnImg[0];
	delete this->btnImg[1];
}

// This loads the pixmaps used for pressed/released state.
void
JPushButton::loadPixmaps (const char* pixname) {
//	this->btnImg[0] = new QImage (QString(pixname) + REL_PREFIX + ".png");
//	this->btnImg[1] = new QImage (QString(pixname) + PRESS_PREFIX + ".png");

	QString pressedPixmapPath, releasedPixmapPath;
	
	pressedPixmapPath = Skin::getPath(QString(SKINDIR),
									guiWidget->setPathSkin(),
									QString(pixname) + PRESS_PREFIX + ".png");
	releasedPixmapPath = Skin::getPath(QString(SKINDIR),
									guiWidget->setPathSkin(),
									QString(pixname) + REL_PREFIX + ".png");

	this->btnImg[0] = new QImage (releasedPixmapPath);
	this->btnImg[1] = new QImage (pressedPixmapPath);
}

// This function was derived from QImage::createHeuristicMask()
// It creates the heuristic mask that will allow transparency.
int
JPushButton::MyCreateHeuristicMask (const  QImage & img_XX, QImage &m,
		long transp_col) {
	if (img_XX.isNull()) {
		return -1;
	}

	//if ( img.depth() != 32 )
	//{
		QImage img = img_XX.convertDepth(32);
		//return MyCreateHeuristicMask( img32, m, transp_col);
	//}

	int w = img.width();
	int h = img.height();
	m = QImage(w, h, 1, 2, QImage::LittleEndian);
	m.setColor( 0, 0xffffff );
	m.setColor( 1, 0 );
	m.fill( 0xff );
	QRgb bg;
	// User defined transp. color not set. get it from pixel at 0x0
	if (transp_col == -1) {
		// Pixel im Punkt 0x0
		bg = *((QRgb*)img.scanLine(0)+0) & 0x00ffffff ;
	}

	// Use user defined color for transparency
	else {
		bg = transp_col & 0x00ffffff;
	}


	QRgb *p;
	QRgb p24;
	uchar * mp;
	int x,y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			p = (QRgb *) img.scanLine(y) + x ;

			p24 = (*p & 0x00ffffff );
			if (p24 == bg) {
				mp = m.scanLine(y);

				*(mp + (x >> 3)) &= ~(1 << (x & 7));
			}
			p++;
		}
	}
	return 0;
}

// This slot is toggled when the button is pressed
// It changes the picture.
void
JPushButton::iAmPressed (void) {
	setPixmap (*(this->btnImg[1]));
	setMask (mask[1]);
}

// This slot is toggled when the button is released
// It changes the picture.
void
JPushButton::iAmReleased (void) {
	setPixmap (*(this->btnImg[0]));
	setMask (mask[0]);
}

// Mouse button released
void 
JPushButton::mousePressEvent (QMouseEvent *e) {
	switch (e->button()) {
	case Qt::LeftButton:
		iAmPressed();
		break;

	default:
		e->ignore();
		break;
	}
}

// Mouse button released
void 
JPushButton::mouseReleaseEvent (QMouseEvent *e) {
	switch (e->button()) {
	case Qt::LeftButton:
		iAmReleased();
		// Emulate the left mouse click
		if (this->rect().contains(e->pos())) {
			emit clicked();
		}
		break;

	default:
		e->ignore();
		break;
	}
}

void 
JPushButton::mouseMoveEvent (QMouseEvent *e) {
	e->accept();
}

#include "jpushbuttonmoc.cpp"
