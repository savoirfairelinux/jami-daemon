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
 
#include <qpainter.h>
#include <qstyle.h>

#include "qjlistboxpixmap.h"

QjListBoxPixmap::QjListBoxPixmap( PixmapLocation location, const QPixmap &pixmap, const QString &text, QListBox *listbox ) : QListBoxItem( listbox )
{
	m_location = location;
	m_pixmap = pixmap;
	setText( text );
}

QjListBoxPixmap::QjListBoxPixmap( PixmapLocation location, const QPixmap &pixmap, const QString &text, QListBox *listbox, QListBoxItem *after ) : QListBoxItem( listbox, after )
{
	m_location = location;
	m_pixmap = pixmap;
	setText( text );
}
	
QjListBoxPixmap::PixmapLocation QjListBoxPixmap::location() const
{
	return m_location;
}

const QPixmap *QjListBoxPixmap::pixmap() const
{
	return &m_pixmap;
}

void QjListBoxPixmap::setPixmap( const QPixmap &pixmap )
{
	m_pixmap = pixmap;
	listBox()->repaint();
}

int QjListBoxPixmap::height( const QListBox *lb ) const
{
	switch( m_location )
	{
		case Above:
		case Under:
		
			return 6 + m_pixmap.height() + lb->fontMetrics().height();
			
		case Left:
		case Right:
		
			if( m_pixmap.height() > lb->fontMetrics().height() )
				return 4 + m_pixmap.height();
			else
				return 4 + lb->fontMetrics().height();
			
		default:
			return 0;
	}
}

int QjListBoxPixmap::width( const QListBox *lb ) const
{
	int tw;

	switch( m_location )
	{
		case Above:
		case Under:
		
			tw = lb->fontMetrics().width( text() );
			
			if( tw > m_pixmap.width() )
				return 4 + tw;
			else
				return 4 + m_pixmap.width();
		
		case Left:
		case Right:
		
			return 6 + m_pixmap.width() + lb->fontMetrics().width( text() );
			
		default:
			return 0;
	}
}
	
void QjListBoxPixmap::setLocation( PixmapLocation location )
{
	if( m_location == location )
		return;
		
	m_location = location;
	listBox()->repaint();
}
	
void QjListBoxPixmap::paint( QPainter *p )
{
	if( !( listBox() && listBox()->viewport() == p->device() ) )
		return;

	QRect r( 0, 0, listBox()->width(), height( listBox() ) );

	if( isSelected() )
		p->eraseRect( r );
	
	int tw = listBox()->fontMetrics().width( text() );
	int th = listBox()->fontMetrics().height();
	int pw = m_pixmap.width();
	int ph = m_pixmap.height();
	int xo = (listBox()->width() - width( listBox() ))/2;
	int tyo = listBox()->fontMetrics().ascent();
	
	switch( m_location )
	{
		case Above:
			p->drawText( (listBox()->width()-tw)/2, ph+4+tyo, text() );
			p->drawPixmap( (listBox()->width()-pw)/2, 2, m_pixmap );
		
			break;
		case Under:
			p->drawText( (listBox()->width()-tw)/2, 2+tyo, text() ); 
			p->drawPixmap( (listBox()->width()-pw)/2, 4+th, m_pixmap );
			
		  break;
		case Left:
			p->drawText( xo+2+pw, (height( listBox() )-th)/2+tyo, text() );
			p->drawPixmap( xo, (height( listBox() )-ph)/2, m_pixmap );
			
		  break;
		case Right:		
			p->drawText( xo, (height( listBox() )-th)/2+tyo, text() );
			p->drawPixmap( xo+2+tw, (height( listBox() )-ph)/2, m_pixmap );
			
			break;
	}

	if( isCurrent() )
		listBox()->style().drawPrimitive( QStyle::PE_FocusRect, p, r, listBox()->colorGroup() );
}
