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

#ifndef __MYDISPLAY_H__
#define __MYDISPLAY_H__

#include <qevent.h>
#include <qimage.h>
#include <qsettings.h>
#include <qstring.h>
#include <qthread.h>
#include <qwidget.h>

#include "CDataFile.h"
#include "global.h"

#define FREE_STATUS		QObject::tr("Welcome to SFLPhone")
#define ONHOLD_STATUS	QObject::tr("On hold ...")

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
public:
	MyDisplay 	();
	MyDisplay 	(QWidget *, const char *, QtGUIMainWindow *);
	~MyDisplay	();
	
	void 		setStatus (const QString &);
	QString &	getStatus (void);
	void 		setTimer (const QString &);
	QString &	getTimer (void);
	bool 		inFunction;

	QString		       *textBuffer;		
	QString		       *time;

public slots:
	void	 appendText	(const QString &);
	void	 appendText	(const char *);
	void	 appendText	(const QChar &);
	void	 clear		(void);
	void	 clearBuffer(void);
	void	 clear		(const QString &);
	void 	 backspace	(void);

protected:
	void	 paintEvent (QPaintEvent *);

private:
	QImage		 		centerImage;	// text zone
	QImage				overImage;
	QString			   *status;	
	MyDisplayThread	   *animationThread;
	QtGUIMainWindow	   *qtgui;	
	
	CDataFile 	 		ExistingDF;		// Configuration file

	void		initGraphics	(void);
	void		initText		(void);
	void		renderText		(QPainter &, QFontMetrics &, QString &);
	void		renderStatus	(QPainter &, QFontMetrics &, QString &);
	void		renderTime 		(QPainter &, QFontMetrics &);
};

#endif	// __MYDISPLAY_H__

// EOF
