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


#include <qapplication.h>
#include <qdatetime.h>
#include <qfont.h>
#include <qpainter.h>
#include <qstring.h>
#include <stdio.h>
#include <math.h>

#include "mydisplay.h"
#include "skin.h"
#include "qtGUImainwindow.h"

#define TABULATION	1
#define	TEXT_LINE	2
#define	TIMER_LINE	4
#define FONT_FAMILY	"Courier"
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
	this->qtgui = qtgui;
	this->initGraphics();
	this->initText();
}

/**
 * Destructor.
 */
MyDisplay::~MyDisplay (void) {
	delete animationThread;
	delete textBuffer;
	delete qtgui;
	delete status;
	delete time;
}

/**
 * Reimplementation of paintEvent() to render text in the display with 
 * specified font, a fixed-size font.
 */
void
MyDisplay::paintEvent (QPaintEvent *e) {
	if (e);
		
	QPixmap pm(centerImage.width(),centerImage.height());
	
	// Painter settings 
	QFont font(FONT_FAMILY, FONT_SIZE);
	font.setBold(true);
	QFontMetrics fm(font);

	QPainter p;
	p.begin(&pm);
	p.setFont(font);
	
	// Text image (static)
	bitBlt (&pm, 0, 0,
		&(this->centerImage), 0, 0,
		this->centerImage.width(),
		this->centerImage.height(),
		Qt::CopyROP);
	
	// Display text render
	renderText (p, fm, *textBuffer);
	// Display status
	renderStatus (p, fm, *status);
	// Display time
	renderTime (p, fm);

	// Overscreen image
	bitBlt (&pm, 0, 0,
		&(this->overImage), 0, 0,
		this->overImage.width(),
		this->overImage.height(),
		Qt::CopyROP);
		
	p.end();
	
	// "Swap" buffers
	bitBlt (this, 0, 0,
		&pm, 0, 0,
		this->centerImage.width(),
		this->centerImage.height(),
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
		qtgui->setPathSkin(),
	   	QString(PIXMAP_SCREEN)));
	this->centerImage = pixcenter->convertToImage();
	delete pixcenter;

	// Load overscreen image
	QPixmap *pixover = new QPixmap(Skin::getPath(QString(SKINDIR), 
		qtgui->setPathSkin(),
	   	QString(PIXMAP_OVERSCREEN)));
	this->overImage = pixover->convertToImage();
	delete pixover;
	
	// Adjust our size to the picture's one
	this->setGeometry (0, 0, centerImage.width(), centerImage.height());

	// Graphics engine animation thread
	this->animationThread = new MyDisplayThread (this);
	this->animationThread->start();
}

/**
 * Create new buffer to compose.
 */
void
MyDisplay::initText (void) {
	this->textBuffer = new QString ();
	// Init status to free-status
	this->status = new QString(FREE_STATUS);
	// No display call-timer
	inFunction = false;
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
	static unsigned int	cpl = centerImage.width() / fm.width("a");
	
	// If the texts length is greater than the screen, we have to
	// scroll the WHOLE text towards the left.
	uint	 x_offset = 0;
	uint	 extra_chars = 0;
	QString	 backup_string(".");
	
	// If the string is larger than the screen...
	if (fm.width(str) > (centerImage.width() - 5)) {
		extra_chars = str.length() - cpl;
		x_offset = fm.width(str[0]) * extra_chars;
		x_offset = fm.width(str[0]) * extra_chars;

		// Hack the scrolled string to inform the user
		backup_string[0] = str[extra_chars];
		str.replace (extra_chars, backup_string.length(),QString("<"));
	}
	
	// Render text
	painter.drawText (TABULATION - x_offset, fm.height() * TEXT_LINE, str);

	if (fm.width(str) > (centerImage.width() - 5)) {
		// Restore initial string.
		str.replace(extra_chars, backup_string.length(), backup_string);
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
	if (inFunction) {
		elapse = qtgui->callmanager->phLines[qtgui->currentLineNumber]->timer->elapsed() / 1000;
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
		
		time = new QString(minStr + ":" + secStr);

	} else {
		// If no conversation, display the current time
		time = new QString(QDateTime::currentDateTime().toString("hh:mm"));
		
		/** For english time format:
		 * time = new QString(QDateTime::currentDateTime().toString("h:m AP"));
		 */
		
		// Handle time separator blinking.
		static unsigned int i = 0;
		if (i++ % 12 > 5) {
			time->replace (':', ' ');
		}
	}
	painter.drawText (TABULATION, fm.height() * TIMER_LINE, *time);
}

/**
 * To modify status.
 *
 * @param	status
 */
void
MyDisplay::setStatus (const QString &status) {
	// Free if previously allocated
	if (this->status != NULL) {
		delete this->status;
	}

	this->status = new QString (status);
}

/**
 * To access status.
 *
 *@return	status
 */
QString &
MyDisplay::getStatus (void) {
	return *status;
}

/**
 * To modify timer line.
 *
 * @param	time
 */
void
MyDisplay::setTimer (const QString &time) {
	// Free if previously allocated
	if (this->time != NULL) {
		delete this->time;
	}
	this->time = new QString (time);
}

/**
 * To access timer.
 *
 * @return	time
 */
QString &
MyDisplay::getTimer (void) {
	return *time;
}

///////////////////////////////////////////////////////////////////////////////
// Implementation of the public slots                                        //
///////////////////////////////////////////////////////////////////////////////
void 
MyDisplay::appendText (const QString &strToAdd) {
	this->textBuffer->append(strToAdd);
}

void
MyDisplay::appendText (const char *text) {
	this->textBuffer->append(QString(text));
}

void
MyDisplay::appendText (const QChar &key) {
	this->textBuffer->append(QChar(key));
}

void
MyDisplay::clear (void) {
	// Remove everything in the buffer.
	this->textBuffer->remove(0, this->textBuffer->length());
	setStatus(QString(FREE_STATUS));
}

void
MyDisplay::clear (const QString &newstatus) {
	// Remove everything in the buffer and set the new status.
	this->textBuffer->remove(0, this->textBuffer->length());
	setStatus(newstatus);
}

void
MyDisplay::backspace (void) {
	// Remove the last char of the string
	this->textBuffer->remove(this->textBuffer->length() - 1, 1);
}

// EOF
