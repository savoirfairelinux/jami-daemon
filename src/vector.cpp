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

#include "vector.h"
#include "point.h"
#include "transqwidget.h"

Vector::Vector (QtGUIMainWindow* gui, const char* name, Point* pt) {
	this->x = pt->getX (name);
	this->y = pt->getY (name);
	this->variation = pt->getVariation (name);
	this->direction = pt->getDirection (name);

	this->gui = (TransQWidget*)gui;
}

Vector::~Vector (void) {
}

/**
 * Return the y-offset of the gui
 */
int
Vector::offsetY (void) {
	return (gui->getGlobalMouseY() - gui->getMouseY());
}

/**
 * Return the x-offset of the gui
 */
int
Vector::offsetX (void) {
	return (gui->getGlobalMouseX() - gui->getMouseX());
}
