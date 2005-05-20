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

#ifndef __VECTOR_H__
#define __VECTOR_H__

#include "point.h"

class TransQWidget;
class QtGUIMainWindow;

class Vector {
public:
	Vector (QtGUIMainWindow* = 0, const char* = 0, Point* = 0);
	~Vector (void);

	inline int X (void) { return x; }
	inline int Y (void) { return y; }
	inline int Direction (void) { return direction; }
	inline int Variation (void) { return variation; }

	int offsetY (void);
	int offsetX (void);

private:
	int x;
	int y;
	int direction;
	int variation;

	TransQWidget* gui;
};

#endif // __VECTOR_H__
