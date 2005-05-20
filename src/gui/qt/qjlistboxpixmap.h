/*
  Copyright(C)2004 Johan Thelin
	johan.thelin -at- digitalfanatics.org
	
	Visit: http://www.digitalfanatics.org/e8johan/projects/jseries/index.html

  This file is part of the JSeries.

  JSeries is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  JSeries is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with JSeries; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef QjLISTBOXPIXMAP_H
#define QjLISTBOXPIXMAP_H

#include <qlistbox.h>
#include <qstring.h>
#include <qpixmap.h>

/** \brief The JPixmapItem is a listbox item showing a pixmap and a text. The location of the pixmap in relation to the text can be altered.
  *
	* \image html jpmi.png
	* The location of the pixmap in relation to the text can be altered using the location and setLocation members.
	*/
class QjListBoxPixmap : public QListBoxItem
{
public:
	/** Specifies the location of the pixmap in relation to the text. */
	enum PixmapLocation 
		{ Above, /**< The pixmap is above the text. */
		  Under, /**< The pixmap is under the text. */
			Left,  /**< The pixmap is to the left of the text. */
			Right  /**< The pixmap is to the right of the text. */
		};

  /** Creates a JPixmapItem. */
	QjListBoxPixmap( PixmapLocation location, const QPixmap &pixmap, const QString &text, QListBox *listbox=0 );
	/** Creates a JPixmapItem at a certain position in the listbox. */
	QjListBoxPixmap( PixmapLocation location, const QPixmap &pixmap, const QString &text, QListBox *listbox, QListBoxItem *after );
	
	/** Returns the pixmap location in relation to the text. */
	PixmapLocation location() const;
	/** Sets the pixmap location in relation to the text. This does not generate a re-paint of the listbox. */
	void setLocation( PixmapLocation );

	/** Returns the pixmap. */
	const QPixmap *pixmap() const;	
	/** Sets the pixmap. This does not generate a re-paint of the listbox. */
	void setPixmap( const QPixmap &pixmap );
	
	int height( const QListBox *lb ) const;
	int width( const QListBox *lb ) const;
	
protected:
	void paint( QPainter *p );
	
private:
	QPixmap m_pixmap;
	PixmapLocation m_location;
};

#endif // QjLISTBOXPIXMAP_H
