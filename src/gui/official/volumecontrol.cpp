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

#include <qapplication.h>

#include "point.h"
#include "qtGUImainwindow.h"
#include "vector.h"
#include "volumecontrol.h"
#include "../../skin.h"

VolumeControl::VolumeControl (QWidget *parent, const char *name, 
		const char* pixname, Vector *v) : JPushButton(parent, name, pixname) {
	this->vect = v;
	volumeValue = 0;
}

VolumeControl::~VolumeControl (void) {
}

int 
VolumeControl::offset (int dir) {	
	if (dir == VERTICAL) {
		if (vect->offsetY() == 0) 
			return mouse_y + MAIN_INITIAL_POSITION;
		else if (vect->offsetY() > 0) 
			return mouse_y + vect->offsetY();
		else 
			return 0;
	} else if (dir == HORIZONTAL) {
		if (vect->offsetX() == 0) 
			return mouse_x + MAIN_INITIAL_POSITION;
		else if (vect->offsetX() > 0) 
			return mouse_x + vect->offsetX();
		else 
			return 0;
	} else
		return -1;
}

void
VolumeControl::mouseMoveEvent (QMouseEvent *e) {
	if (vect->Direction() == VERTICAL) {
		// If the slider for the volume is vertical		
		QPoint ptref(e->globalX() - mouse_x, e->globalY() - offset(VERTICAL));
		if (ptref.y() >= vect->Variation() and ptref.y() <= vect->Y()) {
			volumeValue = vect->Y() - ptref.y();
			// Emit a signal
			emit setVolumeValue (volumeValue);
			move (QPoint(vect->X(), ptref.y()));
		}
	} 
	if (vect->Direction() == HORIZONTAL) {
		// If the slider for the volume is horizontal
		QPoint ptref(e->globalX() - offset(HORIZONTAL), e->globalY() - mouse_y);
		if (ptref.x() >= vect->X() and ptref.x() <= vect->Variation()) {
			volumeValue = vect->X() - ptref.x();
			// Emit a signal
			emit setVolumeValue (volumeValue);
			move (QPoint(ptref.x(), vect->Y()));
		}
	}
}

void
VolumeControl::mousePressEvent (QMouseEvent *e) {
	mouse_x = e->x();
	mouse_y = e->y();
}

#include "volumecontrolmoc.cpp"

// EOF
