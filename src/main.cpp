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
#include "configurationtree.h"

#include <getopt.h>

#include <qapplication.h>
#include <qmessagebox.h>
#include <qtranslator.h>
#include <qwidget.h>

#include "manager.h"
#include "mydisplay.h"
#include "numerickeypad.h"
#include "skin.h"
#include "qtGUImainwindow.h"


void OptionProcess (int argc,char **argv) ;
QString *pOption ;


int
main (int argc, char **argv) {
	QApplication	a(argc, argv);
	Manager *manager;
	
	Config::setTree(new ConfigurationTree());	
	
	OptionProcess (argc,argv);
	if ( pOption )  
		manager = new Manager(pOption);
	else
		manager = new Manager(new QString());

#if 0
	QTranslator translator (0);
	translator.load("app_fr.qm", ".");
	a.installTranslator (&translator);	
#endif

	a.setMainWidget(manager->gui());
	return a.exec();
}


void OptionProcess (int argc,char **argv) {
int c;

        while (1) {
                int option_index = 0;
                static struct option long_options[] =
                {
                                {"phonenumber", 1, 0, 'p'},
                                {"stun", 1, 0, 's'},
                                {"verbose", 0, 0, 'v'},
                                {"help", 0, 0, 'h'},
                                {0, 0, 0, 0}
                };

                c = getopt_long (argc, argv, "p:s:vh", long_options, &option_index);
                if (c == -1)
                        break;

                switch (c) {
                        case 'v':
                                break;
                        case 'p':
				printf("Phone number to call : %s\n",optarg);
				pOption = new QString(optarg);		
                                break;
                        case 's':
                                break;
                        case '?':
                        case 'h':
                                break;
                        default:
                                printf ("?? caractère de code 0%o ??\n", c);
                }
        }
}





