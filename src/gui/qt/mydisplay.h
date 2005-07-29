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

#ifndef __MYDISPLAY_H__
#define __MYDISPLAY_H__

#include <qevent.h>
#include <qimage.h>
#include <qsettings.h>
#include <qstring.h>
#include <qthread.h>
#include <qtimer.h>
#include <qwidget.h>

#include "../../global.h"

#define FREE_STATUS		"Welcome to SFLPhone"

// Screen animation thread
class MyDisplayThread : public QThread {
public:
	MyDisplayThread (QWidget *);

public:
	virtual void 	 run ();

private:
	QWidget 	*widget;
	bool	 	 alive;
};

class QtGUIMainWindow;

// Display
class MyDisplay : public QWidget {
	Q_OBJECT
public:
	MyDisplay 	();
	MyDisplay 	(QWidget *, const char *, QtGUIMainWindow *);
	~MyDisplay	();
	
	void 		setStatus (const QString &);
	QString &	getStatus (void);
	void 		setTimer (const QString &);
	QString &	getTimer (void);
	void 		setTextBuffer (const QString &);
	QString &	getTextBuffer (void);
	void		initGraphics	(void);

	inline bool getInFunction (void) { return _inFunction; }
	inline void setInFunction (bool b) { _inFunction = b; }

	inline bool getIsScrolling (void) { return _isScrolling; }
	inline void setIsScrolling (bool s) { _isScrolling = s; }

	inline void resetForScrolling (bool r) { _reset = r; }
	inline bool getReset (void) { return _reset; }

public slots:
	void	 appendText	(const QString &);
	void	 appendText	(const char *);
	void	 appendText	(const QChar &);
	void	 clear		(void);
	void	 clearBuffer(void);
	void	 clear		(const QString &);
	void 	 backspace	(void);
	void 	 shift		(void);

protected:
	void	 paintEvent (QPaintEvent *);

private:
	QImage		 		_centerImage;	// text zone
	QImage				_overImage;
	QString*			_status;	
	MyDisplayThread*	_animationThread;
	QtGUIMainWindow*	_qtgui;	
	QString*			_textBuffer;		
	QString*			_time;
	bool 				_inFunction;
	bool				_shift;
	QTimer*				_timerForScrollingText;
	bool 				_isScrolling;
	bool				_reset;
	unsigned int 		len_to_shift;
	
	void		initText		(void);
	void		renderText		(QPainter &, QFontMetrics &, QString &);
	void		renderStatus	(QPainter &, QFontMetrics &, QString &);
	void		renderTime 		(QPainter &, QFontMetrics &);
};

#endif	// __MYDISPLAY_H__

// EOF
