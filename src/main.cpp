/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#include "user_cfg.h"
#include "gui/server/guiserver.h"
#include "gui/guiframework.h"
#include "manager.h"
#include "cstring"
#include "iostream"

int
main (int argc, char **argv) {
  int exit_code = 0;

  if (argc == 2 && strcmp(argv[1], "--help") == 0) {

    std::cout << PROGNAME << " Deamon " << SFLPHONED_VERSION << ", by Savoir-Faire Linux 2004-2005" << std::endl << std::endl;
    std::cout << "USAGE: sflphoned [--help]" << std::endl;
    std::cout << "Parameters: " << std::endl;
    std::cout << "  --help for this message" << std::endl << std::endl;
    std::cout << "See http://www.sflphone.org/ for more information" << std::endl;

  } else {
    GuiFramework *GUI;
    bool initOK = false;
    try {
      Manager::instance().initConfigFile();
      Manager::instance().init();
      initOK = true;
    }
    catch (...) {
      std::cerr << 
    "An exception occured when initializing the system." << 
    std::endl;
      exit_code = -1;
    }
    if (initOK) {
      GUI = &(GUIServer::instance());
      Manager::instance().setGui(GUI);
      exit_code = GUIServer::instance().exec();
    }
  }

  return exit_code;
}

// EOF

