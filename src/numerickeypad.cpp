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

#include "CDataFile.h"
#include "jpushbutton.h"
#include "numerickeypad.h"
#include "qtGUImainwindow.h"
#include "skin.h"


NumericKeypad::NumericKeypad (QWidget *parent, const char *name, WFlags f) 
						: TransQWidget (NULL, name, f) {
	mainWindow = (TransQWidget *)parent;
	this->setCaption("DTMF keypad");
	
	// Load background image phone 
	setbgPixmap (new QPixmap (Skin::getPath(QString(SKINDIR), 
											QtGUIMainWindow::setPathSkin(),
											QString(PIXMAP_KEYPAD))));

	// Transform pixmap to QImage
	setSourceImage ();

	setMaximumSize (getSourceImage().width(), getSourceImage().height());

	// Calculate just one time the transparency mask bit to bit
	transparencyMask ();						

	CDataFile ExistingDF;
	QString skinfilename(Skin::getPath(QString(SKINDIR), 
										QtGUIMainWindow::setPathSkin(),
										QString(FILE_INI)));
	ExistingDF.SetFileName(skinfilename);
	ExistingDF.Load(skinfilename);
	
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
	key0->move (ExistingDF.GetInt("dtmf_0_x","Positions"), 
				ExistingDF.GetInt("dtmf_0_y","Positions"));
	key1->move (ExistingDF.GetInt("dtmf_1_x","Positions"), 
				ExistingDF.GetInt("dtmf_1_y","Positions"));
	key2->move (ExistingDF.GetInt("dtmf_2_x","Positions"), 
				ExistingDF.GetInt("dtmf_2_y","Positions"));
	key3->move (ExistingDF.GetInt("dtmf_3_x","Positions"), 
				ExistingDF.GetInt("dtmf_3_y","Positions"));
	key4->move (ExistingDF.GetInt("dtmf_4_x","Positions"), 
				ExistingDF.GetInt("dtmf_4_y","Positions"));
	key5->move (ExistingDF.GetInt("dtmf_5_x","Positions"), 
				ExistingDF.GetInt("dtmf_5_y","Positions"));
	key6->move (ExistingDF.GetInt("dtmf_6_x","Positions"), 
				ExistingDF.GetInt("dtmf_6_y","Positions"));
	key7->move (ExistingDF.GetInt("dtmf_7_x","Positions"), 
				ExistingDF.GetInt("dtmf_7_y","Positions"));
	key8->move (ExistingDF.GetInt("dtmf_8_x","Positions"), 
				ExistingDF.GetInt("dtmf_8_y","Positions"));
	key9->move (ExistingDF.GetInt("dtmf_9_x","Positions"), 
				ExistingDF.GetInt("dtmf_9_y","Positions"));
	keyStar->move (ExistingDF.GetInt("dtmf_star_x","Positions"), 
				ExistingDF.GetInt("dtmf_star_y","Positions"));
	keyHash->move (ExistingDF.GetInt("dtmf_pound_x","Positions"), 
				ExistingDF.GetInt("dtmf_pound_y","Positions"));
	keyClose->move (ExistingDF.GetInt("dtmf_close_x","Positions"), 
				ExistingDF.GetInt("dtmf_close_y","Positions"));
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



