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
#include <qdatetime.h>
#include <qfont.h>
#include <qpainter.h>
#include <qstring.h>
#include <stdio.h>
#include <math.h>

#include "../../skin.h"
#include "mydisplay.h"
#include "qtGUImainwindow.h"

#define TABULATION	1
#define	TEXT_LINE	2
#define	TIMER_LINE	4
#define FONT_FAMILY	"Courier"
// Others fixed font support "Monospace", "Fixed", "MiscFixed"
#define FONT_SIZE	10

///////////////////////////////////////////////////////////////////////////////
// MyDisplayThread Implementation                                            //
///////////////////////////////////////////////////////////////////////////////

/**
 * Default constructor.
 */
MyDisplayThread::MyDisplayThread (QWidget *thewidget) : QThread () {
	alive = false;
	widget = thewidget;
}

/**
 * Reimplementation of run() to update widget
 */
void
MyDisplayThread::run (void) {
	alive = true;

	while (alive) {
		msleep (120);
		if (widget != NULL) {
			widget->update();
		}
	}
} 


///////////////////////////////////////////////////////////////////////////////
// MyDisplay Implementation                                                  //
///////////////////////////////////////////////////////////////////////////////

/**
 * Default constructor.
 */
MyDisplay::MyDisplay (void) : QWidget () {
	this->initGraphics();
	this->initText();
}

/**
 * Constructor wtih 3 parameters.
 *
 * @param	widget parent
 * 			parent name
 *			QtGUIMainWindow
 */
MyDisplay::MyDisplay (QWidget *parent, const char *name, QtGUIMainWindow* qtgui)
			: QWidget (parent, name)  {
	_qtgui = qtgui;
	this->initGraphics();
	this->initText();

	// For scrolling text
	_timerForScrollingText = new QTimer(this);
	connect (_timerForScrollingText, SIGNAL(timeout()), SLOT(shift()));
	//emit signal every second
	_timerForScrollingText->start(2000); 	
	
	// Graphics engine animation thread
	_animationThread = new MyDisplayThread (this);
	_animationThread->start();
}

/**
 * Destructor.
 */
MyDisplay::~MyDisplay (void) {
	delete _animationThread;
	delete _textBuffer;
	delete _qtgui;
	delete _status;
	delete _time;
	_timerForScrollingText->stop();
	delete _timerForScrollingText;
}

/**
 * Reimplementation of paintEvent() to render text in the display with 
 * specified font, a fixed-size font.
 */
void
MyDisplay::paintEvent (QPaintEvent *e) {
	if (e);
		
	QPixmap pm(_centerImage.width(),_centerImage.height());
	
	// Painter settings 
	QFont font(FONT_FAMILY, FONT_SIZE);
	font.setBold(true);
	QFontMetrics fm(font);

	QPainter p;
	p.begin(&pm);
	p.setFont(font);
	
	// Text image (static)
	bitBlt (&pm, 0, 0,
		&(_centerImage), 0, 0,
		_centerImage.width(),
		_centerImage.height(),
		Qt::CopyROP);
	
	// Display text render
	renderText (p, fm, *_textBuffer);
	// Display status
	renderStatus (p, fm, *_status);
	// Display time
	renderTime (p, fm);

	// Overscreen image
	bitBlt (&pm, 0, 0,
		&(_overImage), 0, 0,
		_overImage.width(),
		_overImage.height(),
		Qt::CopyROP);
		
	p.end();
	
	// "Swap" buffers
	bitBlt (this, 0, 0,
		&pm, 0, 0,
		_centerImage.width(),
		_centerImage.height(),
		Qt::CopyROP);
}

/**
 * Load display image, set geometry and start the thread to redraw LCD display.
 */
void
MyDisplay::initGraphics (void) {
	// This widget is 100% painted by hand
	this->setBackgroundMode (Qt::NoBackground);

	// Load display screen image
	QPixmap *pixcenter = new QPixmap(Skin::getPath(QString(SKINDIR), 
		_qtgui->setPathSkin(),
	   	QString(PIXMAP_SCREEN)));
	_centerImage = pixcenter->convertToImage();
	delete pixcenter;

	// Load overscreen image
	QPixmap *pixover = new QPixmap(Skin::getPath(QString(SKINDIR), 
		_qtgui->setPathSkin(),
	   	QString(PIXMAP_OVERSCREEN)));
	_overImage = pixover->convertToImage();
	delete pixover;
	
	// Adjust our size to the picture's one
	this->setGeometry (0, 0, _centerImage.width(), _centerImage.height());
}

/**
 * Create new buffer to compose.
 */
void
MyDisplay::initText (void) {
	_textBuffer = new QString ();
	// Init status to free-status
	_status = new QString(FREE_STATUS);
	// No display call-timer
	_inFunction = false;
	// No scrolling
	_isScrolling = false;
	_reset = false;
	len_to_shift = 0;
}

/**
 * Draw text in the appropriate position. 
 * Scroll text on the left when text is over than width display.
 * 
 * @param 	painter		
 *			fm			size of font
 *			str			text to draw
 */
void
MyDisplay::renderText (QPainter &painter, QFontMetrics &fm, QString &str) {
	// cpl = how many chars per line.
	static unsigned int	cpl = _centerImage.width() / fm.width("a");
	
	// If the texts length is greater than the screen, we have to
	// scroll the WHOLE text towards the left.
	uint	 x_offset = 0;
	uint	 extra_chars = 0;
	QString	 backup_string(".");
	QString str_tmp = str;

	// If call is closed, reset len_to_shift to 0
	if (getReset()) {
		len_to_shift = 0;
	}
	
	// If don't need scrolling message
	if (!getIsScrolling()) {
		// If the string is larger than the screen...
		if (fm.width(str) > (_centerImage.width() - 5)) {
			extra_chars = str.length() - cpl;
			x_offset = fm.width(str[0]) * extra_chars;

			// Hack the scrolled string to inform the user
			backup_string[0] = str[extra_chars];
			str.replace (extra_chars, backup_string.length(),QString("<"));
		}
		// Render text
		painter.drawText (TABULATION - x_offset, fm.height() * TEXT_LINE, str);
		if (fm.width(str) > (_centerImage.width() - 5)) {
			// Restore initial string.
			str.replace(extra_chars, backup_string.length(), backup_string);
		}
	} else {
		// if need scrolling text
		if (_shift) {
			len_to_shift += 1;
		}

		if (str.length() > cpl) {
			str_tmp = str.left(len_to_shift);
		} 
		if (len_to_shift == cpl) {
			str_tmp = str.right(len_to_shift);
		}
		if (len_to_shift == str.length()*2) {
			str_tmp = str.left(len_to_shift);
			len_to_shift = 0;
		}

		painter.drawText (TABULATION + _centerImage.width() - 
				len_to_shift * fm.width(str[0]), 
				fm.height() * TEXT_LINE, str_tmp);
	}
	
}

void
MyDisplay::shift (void)
{
	if (getReset()) {
		_shift = false;
		len_to_shift = 0;
	} else {
		_shift = true;
	}
}

/**
 * Draw status text.
 * 
 * @param 	painter		
 *			fm			size of font
 *			str			text to draw
 */
void
MyDisplay::renderStatus (QPainter &painter, QFontMetrics &fm, QString &str) {
	painter.drawText (TABULATION, fm.height(), str);
}

/**
 * Render current time when line is free, or call timer when line is busy.
 * 
 * @param 	painter		
 *			fm			size of font
 */
void
MyDisplay::renderTime (QPainter &painter, QFontMetrics &fm) {
	static unsigned int minute = 0;
	static unsigned int elapse;
	QString minStr,secStr;

	// If conversation, display call-timer
	if (_inFunction) {
		elapse = _qtgui->getElapse();
		// To calculate minutes
		if (elapse % 60 == 0) {
			minute = elapse / 60;
			elapse = elapse - minute * 60;
		}
		// Two digits for seconds
		if ((elapse % 60) < 10) {
			secStr = (QString("0" + QString::number(elapse % 60)));
		} else {
			secStr = QString::number(elapse % 60);
		}
		// Two digits for minutes
		if (minute < 10) {
			minStr = (QString("0" + QString::number(minute)));
		} else {
			minStr = QString::number(minute);
		}
		
		_time = new QString(minStr + ":" + secStr);

	} else {
		// If no conversation, display the current time
		_time = new QString(QDateTime::currentDateTime().toString("hh:mm"));
		
		/** For english time format:
		 * time = new QString(QDateTime::currentDateTime().toString("h:m AP"));
		 */
		
		// Handle time separator blinking.
		static unsigned int i = 0;
		if (i++ % 12 > 5) {
			_time->replace (':', ' ');
		}
	}
	painter.drawText (TABULATION, fm.height() * TIMER_LINE, *_time);
}

/**
 * To modify status.
 *
 * @param	status
 */
void
MyDisplay::setStatus (const QString &status) {
	// Free if previously allocated
	if (_status != NULL) {
		delete _status;
	}

	_status = new QString (status);
}

/**
 * To access status.
 *
 *@return	status
 */
QString &
MyDisplay::getStatus (void) {
	return *_status;
}

/**
 * To modify timer line.
 *
 * @param	time
 */
void
MyDisplay::setTimer (const QString &time) {
	// Free if previously allocated
	if (_time != NULL) {
		delete _time;
	}
	_time = new QString (time);
}

/**
 * To access timer.
 *
 * @return	time
 */
QString &
MyDisplay::getTimer (void) {
	return *_time;
}

/**
 * To modify text buffer.
 *
 * @param	text buffer
 */
void
MyDisplay::setTextBuffer (const QString& text) {
	// Free if previously allocated
	if (_textBuffer != NULL) {
		delete _textBuffer;
	}
	_textBuffer = new QString (text);
}

/**
 * To access text buffer.
 *
 * @return	text buffer
 */
QString &
MyDisplay::getTextBuffer (void) {
	return *_textBuffer;
}

///////////////////////////////////////////////////////////////////////////////
// Implementation of the public slots                                        //
///////////////////////////////////////////////////////////////////////////////
void 
MyDisplay::appendText (const QString &strToAdd) {
	_textBuffer->append(strToAdd);
}

void
MyDisplay::appendText (const char *text) {
	_textBuffer->append(QString(text));
}

void
MyDisplay::appendText (const QChar &key) {
	_textBuffer->append(QChar(key));
}

void
MyDisplay::clear (void) {
	// Remove everything in the buffer.
	_textBuffer->remove(0, _textBuffer->length());
	setStatus(QString(FREE_STATUS));
}

void
MyDisplay::clearBuffer (void) {
	_textBuffer->remove(0, _textBuffer->length());
}

void
MyDisplay::clear (const QString &newstatus) {
	// Remove everything in the buffer and set the new status.
	_textBuffer->remove(0, _textBuffer->length());
	setStatus(newstatus);
}

void
MyDisplay::backspace (void) {
	// Remove the last char of the string
	_textBuffer->remove(_textBuffer->length() - 1, 1);
}

// EOF
