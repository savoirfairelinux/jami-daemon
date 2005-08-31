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

#define GUI_QT // remove when others UI are implemented

#if defined(GUI_QT)
# include <qapplication.h>
# include "gui/qt/qtGUImainwindow.h"
#elif defined(GUI_TEXT1)
# error "GUI_TEXT1 not implemented yet."
#elif defined(GUI_COCOA)
# error "GUI_COCOA not implemented yet."
#endif

#include "gui/guiframework.h"
#include "configuration.h"
#include "configurationtree.h"
#include "manager.h"


int
main (int argc, char **argv) {
  int exit_code = 0;
  Config::setTree(new ConfigurationTree());	
  GuiFramework *GUI;

#if defined(GUI_QT)
  {
    QApplication a(argc, argv);
    Manager::instance().initConfigFile();		

    try {
      Manager::instance().init();		
    }
    catch (const exception &e) {
      std::cerr << e.what() << std::endl;
    }
    catch (...) { 
      std::cerr << 
	"An unknown exception occured when initializing the system." << 
	std::endl;
    }

    GUI = new QtGUIMainWindow (0, 0 ,
			       Qt::WDestructiveClose |
			       Qt::WStyle_Customize |
			       Qt::WStyle_NoBorder);
    Manager::instance().setGui(GUI);
    
    a.setMainWidget((QtGUIMainWindow*)GUI);
    exit_code = a.exec();
    Manager::instance().terminate();
#endif
  }

  return exit_code;
}


// EOF
