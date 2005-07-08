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

#ifndef __VOLUME_CONTROL_H__
#define __VOLUME_CONTROL_H__

#include "jpushbutton.h"

class Vector;
class VolumeControl : public JPushButton {
	Q_OBJECT
public:
	VolumeControl (QWidget* = 0, const char* = 0, const char* = 0, Vector* = 0);
	~VolumeControl (void);

	int		getValue		(void) { return volumeValue; }
	inline	void setValue	(int val) { volumeValue = val; }
	Vector*	vect;	
signals:
	void	setVolumeValue 	(int);
private:
	
	void 	mouseMoveEvent	(QMouseEvent*);
	void 	mousePressEvent	(QMouseEvent*);
	int	 	offset			(int);

	int 	mouse_x,
			mouse_y;
	int 	volumeValue;
};

#endif // __VOLUME_CONTROL_H__
