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
#include <qmessagebox.h>
#include <qtranslator.h>
#include <qwidget.h>

#include "manager.h"
#include "mydisplay.h"
#include "numerickeypad.h"
#include "skin.h"
#include "qtGUImainwindow.h"

int
main (int argc, char **argv) {
	QApplication	a(argc, argv);
	
	Manager *manager = new Manager();

#if 0
	QTranslator translator (0);
	translator.load("app_fr.qm", ".");
	a.installTranslator (&translator);	
#endif

	a.setMainWidget(manager->gui());
	return a.exec();
}
