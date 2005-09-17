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

#ifndef __TRANS_QWIDGET_H__
#define __TRANS_QWIDGET_H__

#include <qbitmap.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qwidget.h>

class TransQWidget : public QWidget {
public:
	TransQWidget	(QWidget* = 0, const char* = 0, WFlags = 0);
	~TransQWidget	(void) {delete pixmap;};
	
	QImage		 getSourceImage		(void);	

	inline
	int			 getMouseX			(void) { return mouse_x; };
	inline
	int			 getMouseY			(void) { return mouse_y; };
	inline
	int			 getGlobalMouseX	(void) { return global_x; };
	inline
	int			 getGlobalMouseY	(void) { return global_y; };
	inline
	bool		 getMoved			(void) { return _moved; };

protected:
	QPixmap 	*pixmap;

	void 		 setbgPixmap		(QPixmap *);
	void 		 setSourceImage 	(void);
	void		 transparencyMask	(void);
		
	// To handle the tranparency
	void 		 paintEvent 		(QPaintEvent *);

	// To handle the drag window with mouse event
	void 		 mouseMoveEvent 	(QMouseEvent *);
	void 		 mousePressEvent	(QMouseEvent *);

private:
	// Private member variables
	QImage 		 SourceImage;
	QImage 		 ShowedImage;
	QBitmap 	 ImageMask;

	int 		 mouse_x, 
				 mouse_y;
	int 		 global_x, 
				 global_y;
	bool		 _moved;
};


#endif // __TRANS_QWIDGET_H__
