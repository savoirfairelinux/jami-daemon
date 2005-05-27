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

#include <getopt.h>
#include "user_cfg.h"
#ifdef QT_GUI
#include <qapplication.h>
#include "gui/qt/qtGUImainwindow.h"
#endif

#include "configuration.h"
#include "configurationtree.h"
#include "manager.h"


int OptionProcess (int argc,char **argv, Manager* manager);

int
main (int argc, char **argv) {
	Manager* manager;
	Config::setTree(new ConfigurationTree());	
	manager = new Manager();

	// Faire partir la gui selon l'option choisie
#if 1
		QApplication a(argc, argv);
		QtGUIMainWindow *qtgui = new QtGUIMainWindow (0, 0 ,
									Qt::WDestructiveClose |
									Qt::WStyle_Customize |
									Qt::WStyle_NoBorder,
									manager);
		manager->setGui(qtgui);
		manager->init();		
		
		a.setMainWidget(qtgui);
		return a.exec();
#endif
	int ret = OptionProcess (argc,argv, manager);
	return ret;
}

int OptionProcess (int argc,char **argv, Manager* manager) 
{
	int c;

	while (1) {
		int option_index = 0;
		static struct option long_options[] =
		{
			{"ui", 1, 0, 'i'},
			{"phonenumber", 1, 0, 'p'},
			{"stun", 1, 0, 's'},
			{"verbose", 0, 0, 'v'},
			{"help", 0, 0, 'h'},
			{0, 0, 0, 0}
		};

		c = getopt_long (argc, argv, "i:p:s:vh", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'i':
				{
					string optStr(optarg);
					if (optStr.compare("qt") == 0) {
#ifdef QT_GUI
						QApplication a(argc, argv);
						QtGUIMainWindow *qtgui = new QtGUIMainWindow (0, 0 ,
                    								Qt::WDestructiveClose |
                    								Qt::WStyle_Customize |
                    								Qt::WStyle_NoBorder,
													manager);
						manager->setGui(qtgui);
						manager->init();
						a.setMainWidget(qtgui);
						return a.exec();
#endif
					} else if (optStr.compare("text")) {
					} else {
					}
				}
				break;				
			case 'v':
				break;
			case 'p':
				cout << "Phone number to call : " << optarg << endl;
				break;
			case 's':
				break;
			case '?':
			case 'h':
				cout << "Usage: sflphone [OPTIONS] " << endl
					 << "Valid Options:" << endl
					 << "-i <interface>, --ui=<interface>" << endl
				 	 << "	Select an user interface (right now just qt interface is available)" << endl
					 << "-v, --verbose" << endl
					 << "	Display all messages for debbugging" << endl
					 << "-p <number>, --phonenumber=<number>" << endl
					 << "	Compose directly the phone number that you want"
					 	<< endl
					 << "-h, --help" << endl
					 << "	Display help options message" << endl;
				break;
			default:
				cout << "Option " << c << "doesn't exist" <<endl;
				break;
		}
	}
	return 0;
}




