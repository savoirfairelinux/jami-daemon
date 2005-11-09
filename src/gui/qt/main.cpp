/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <iostream>
#include <stdexcept>
#include <qcolor.h>
#include <qsplashscreen.h>
#include <qstring.h>
#include <qtextcodec.h>
#include <qtimer.h>
#include <qtranslator.h>

#include "PhoneLineManager.hpp"
#include "SFLPhoneApp.hpp"
#include "SFLPhoneWindow.hpp"
#include "TransparentWidget.hpp"

int main(int argc, char **argv)
{
  SFLPhoneApp app(argc, argv);

  QSplashScreen *splash = new QSplashScreen(TransparentWidget::retreive("splash.png"));
  splash->show();

  // translation file for Qt
  QTranslator qt(NULL);
  qt.load( QString( "qt_" ) + QTextCodec::locale(), "." );
  app.installTranslator( &qt );
  
  
  QTranslator myapp(NULL);
  myapp.load( QString( "sflphone-qt_" ) + QTextCodec::locale(), "." );
  app.installTranslator( &myapp );
    
  SFLPhoneWindow* sfl = new SFLPhoneWindow();
  app.initConnections(sfl);
#ifndef QT3_SUPPORT
  app.setMainWidget(sfl);
#endif

  app.launch();
  PhoneLineManager::instance().connect();
  //splash->finish(sfl);
  //sfl->show();
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(connected()),
		   splash, SLOT(hide()));
  //   QTimer *timer = new QTimer(sfl);
  //   QObject::connect(timer, SIGNAL(timeout()),
  // 		   sfl, SLOT(show()));
//   QObject::connect(timer, SIGNAL(timeout()),
// 		   splash, SLOT(close()));
  
//   timer->start(1500, true);
  
  //delete splash;
  return app.exec();
}
