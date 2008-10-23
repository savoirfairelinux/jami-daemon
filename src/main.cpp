/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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
#include <string>
//#include "config.h"
#include "global.h"

#include "user_cfg.h"
#include "dbus/dbusmanager.h"
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
    FILE *fp;
    char homepid[128];
    
    unsigned int iPid = getpid();
    char cPid[64], cOldPid[64];
    sprintf(cPid,"%d", iPid);

    sprintf(homepid, "%s/.%s/%s", HOMEDIR, PROGDIR, PIDFILE);

    if( (fp = fopen(homepid,"r")) == NULL ){
	//PID file doesn't exists, create and write pid in it
	if( (fp = fopen(homepid,"w")) == NULL ){ 
 	     fprintf(stderr, _("Creating PID file %s failed. Exited.\n"), homepid);
	     exit(-1);
	} else {
	     fputs(cPid , fp );
             fclose( fp );
        }
    } else {
       // PID file exists. Check the former process still alive or not. If alive, kill it and start a new one.
       fgets( cOldPid, 64, fp );
       fclose(fp);
       if (kill(atoi(cOldPid), 0) == SUCCESS) {
 	     fprintf(stderr, _("There is already a sflphoned daemon running in the system. Starting Failed.\n"));
	     exit(-1);
       } else {
       	     if( (fp = fopen(homepid,"w")) == NULL ){ 
             	fprintf(stderr, _("Writing to PID file %s failed. Exited.\n"), homepid);
	 	exit(-1);
             } else {
             	fputs(cPid , fp );
             	fclose( fp );
             }
 	
	}
    }
	
    int sessionPort = 0;
    if (argc == 2) {
      char* ptrPort = strstr(argv[1], "--port=");
      if (ptrPort != 0) {
         sessionPort = atoi(ptrPort+7);
      }
    }
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
      Manager::instance().setDBusManager(&DBusManager::instance());
      exit_code = DBusManager::instance().exec();  // UI Loop
    }
  }

  return exit_code;
}

// EOF

