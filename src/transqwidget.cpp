/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
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

#include <qpoint.h>

#include "transqwidget.h"

TransQWidget::TransQWidget (QWidget *parent, const char *name, WFlags f) 
						: QWidget (parent, name, f) {
	pixmap = NULL;
	_moved = false;
}

void
TransQWidget::setbgPixmap (QPixmap *pix) {
	this->pixmap = pix;
}

void
TransQWidget::setSourceImage (void) {
	this->SourceImage = pixmap->convertToImage ();
}

QImage
TransQWidget::getSourceImage (void) {
	return this->SourceImage;
}

void
TransQWidget::transparencyMask (void) {
	this->ShowedImage = SourceImage;
	this->ImageMask = ShowedImage.createAlphaMask ();
}

/**
 * Reimplementation of paintEvent() to handle the transparency of the 
 * widget.
 * This method is called everytime when the widget is redrawn.
 */
void
TransQWidget::paintEvent (QPaintEvent *e) {
	if (e);// boring warning
	bitBlt (this, 0, 0, &ShowedImage, 0, Qt::CopyROP);
	setMask (ImageMask);
}

/**
 * Reimplementation of mouseMoveEvent() to handle the borderless window drag.
 */
void
TransQWidget::mouseMoveEvent (QMouseEvent *e) {
	move (e->globalPos() - QPoint(mouse_x, mouse_y));
	global_x = e->globalX();
	global_y = e->globalY();
	_moved = true;
}

/**
 * Reimplementation of mousePressEvent() to register mouse positions.
 */
void
TransQWidget::mousePressEvent (QMouseEvent *e) {
	mouse_x = e->x();
	mouse_y = e->y();
}

