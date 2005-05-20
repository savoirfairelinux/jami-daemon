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

#include <string>
#include <qapplication.h>

#include "jpushbutton.h"
#include "numerickeypad.h"
#include "qtGUImainwindow.h"
#include "../../skin.h"

using namespace std;


NumericKeypad::NumericKeypad (QWidget *parent, const char *name, WFlags f) 
						: TransQWidget (NULL, name, f) {
	mainWindow = (TransQWidget *)parent;
	gui = (QtGUIMainWindow*)parent;
	this->setCaption("DTMF keypad");
	
	// Load background image phone 
	setbgPixmap (new QPixmap (Skin::getPath(QString(SKINDIR), 
											gui->setPathSkin(),
											QString(PIXMAP_KEYPAD))));

	// Transform pixmap to QImage
	setSourceImage ();

	setMaximumSize (getSourceImage().width(), getSourceImage().height());

	// Calculate just one time the transparency mask bit to bit
	transparencyMask ();						

	string skinfilename(Skin::getPath(QString(SKINDIR), 
										gui->setPathSkin(),
										QString(FILE_INI)));
	pt = new Point(skinfilename);
	
	// Buttons initialisation
	key0 = new JPushButton(this, NULL, DTMF_0);
	key1 = new JPushButton(this, NULL, DTMF_1);
	key2 = new JPushButton(this, NULL, DTMF_2);
	key3 = new JPushButton(this, NULL, DTMF_3);
	key4 = new JPushButton(this, NULL, DTMF_4);
	key5 = new JPushButton(this, NULL, DTMF_5);
	key6 = new JPushButton(this, NULL, DTMF_6);
	key7 = new JPushButton(this, NULL, DTMF_7);
	key8 = new JPushButton(this, NULL, DTMF_8);
	key9 = new JPushButton(this, NULL, DTMF_9);
	keyStar = new JPushButton(this, NULL, DTMF_STAR);
	keyHash = new JPushButton(this, NULL, DTMF_POUND);
	keyClose = new JPushButton(this, NULL, DTMF_CLOSE);

	// Buttons position 
	key0->move (pt->getX(DTMF_0), pt->getY(DTMF_0));
	key1->move (pt->getX(DTMF_1), pt->getY(DTMF_1));
	key2->move (pt->getX(DTMF_2), pt->getY(DTMF_2));
	key3->move (pt->getX(DTMF_3), pt->getY(DTMF_3));
	key4->move (pt->getX(DTMF_4), pt->getY(DTMF_4));
	key5->move (pt->getX(DTMF_5), pt->getY(DTMF_5));
	key6->move (pt->getX(DTMF_6), pt->getY(DTMF_6));
	key7->move (pt->getX(DTMF_7), pt->getY(DTMF_7));
	key8->move (pt->getX(DTMF_8), pt->getY(DTMF_8));
	key9->move (pt->getX(DTMF_9), pt->getY(DTMF_9));
	keyStar->move (pt->getX(DTMF_STAR), pt->getY(DTMF_STAR));
	keyHash->move (pt->getX(DTMF_POUND), pt->getY(DTMF_POUND));
	keyClose->move (pt->getX(DTMF_CLOSE), pt->getY(DTMF_CLOSE));
}

NumericKeypad::~NumericKeypad (void) {
	delete key0;
	delete key1;
	delete key2;
	delete key3;
	delete key4;
	delete key5;
	delete key6;
	delete key7;
	delete key8;
	delete key9;
	delete keyStar;
	delete keyHash;
	delete keyClose;
	delete pt;
}

void
NumericKeypad::keyPressEvent (QKeyEvent* e) {  
	QApplication::sendEvent (mainWindow, e);

	// TODO: Key appears pressed when done.
	if ((e->key() >= Qt::Key_0 and
			e->key() <= Qt::Key_9) or 
			e->key() == Qt::Key_Asterisk or
			e->key() == Qt::Key_NumberSign) {
		//QApplication::sendEvent(bouton, QEvent(QEvent::MouseButtonPress));
	} else {
		//QApplication::sendEvent (mainWindow, e);
	}
}



