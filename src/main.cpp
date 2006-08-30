/*
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

#include <libintl.h>
#include <cstring>
#include <iostream>

//#include "config.h"
#include "global.h"

#include "user_cfg.h"
#include "gui/server/guiserver.h"
#include "gui/guiframework.h"
#include "manager.h"

#include "audio/audiolayer.h"

int
main (int argc, char **argv) {
  int exit_code = 0;

  //setlocale (LC_ALL, "");
  //bindtextdomain (PACKAGE, LOCALEDIR);
  //textdomain (PACKAGE);

  if (argc == 2 && strcmp(argv[1], _("--help")) == 0) {

    
    printf(_("%1$s Daemon %2$s, by Savoir-Faire Linux 2004-2005\n\n"), 
	   PROGNAME, 
	   SFLPHONED_VERSION);
    printf(_("USAGE: sflphoned [--help]\nParameters: \n  --help\tfor this message\n\n  --port=3999\tchange the session port\n\n"));
    printf(_("See http://www.sflphone.org/ for more information\n"));

  } else {
    int sessionPort = 0;
    if (argc == 2) {
      char* ptrPort = strstr(argv[1], "--port=");
      if (ptrPort != 0) {
         sessionPort = atoi(ptrPort+7);
      }
    }
    GuiFramework *GUI;
    bool initOK = false;
    try {
      Manager::instance().initConfigFile();
      Manager::instance().init();
      initOK = true;
    }
    catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
      exit_code = -1;
    }
    catch (...) {
      fprintf(stderr, _("An exception occured when initializing the system.\n"));
      exit_code = -1;
    }
    if (initOK) {
      GUI = &(GUIServer::instance());
      GUIServer::instance().setSessionPort(sessionPort);
      Manager::instance().setGui(GUI);
      exit_code = GUIServer::instance().exec();
    }
  }

  return exit_code;
}

// EOF

