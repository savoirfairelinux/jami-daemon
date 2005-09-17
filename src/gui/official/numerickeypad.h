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

#ifndef __NUMERIC_KEYPAD_H__
#define __NUMERIC_KEYPAD_H__

#include "jpushbutton.h"
#include "point.h"
#include "transqwidget.h"

class QtGUIMainwindow;

class NumericKeypad : public TransQWidget {
	Q_OBJECT
public:
	// Default Constructor and destructor
	NumericKeypad	(QWidget* = 0, const char* = 0,WFlags = 0);
	~NumericKeypad	(void);

	JPushButton		*key0;
	JPushButton		*key1;
	JPushButton		*key2;
	JPushButton		*key3;
	JPushButton		*key4;
	JPushButton		*key5;
	JPushButton		*key6;
	JPushButton		*key7;
	JPushButton		*key8;
	JPushButton		*key9;
	JPushButton		*keyStar;
	JPushButton		*keyHash;
	JPushButton		*keyClose;

private:
	Point				*pt;	
	TransQWidget*		mainWindow;
	QtGUIMainWindow*	gui;
	void keyPressEvent 	(QKeyEvent*);
};

#endif // __NUMERIC_KEYPAD_H__
