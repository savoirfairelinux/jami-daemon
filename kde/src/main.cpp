/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/

//System
#include <unistd.h>

//Qt
#include <QtGui/QAction>
#include <QApplication>
#include <QtCore/QString>
#include <QtGui/QMenu>
#include <QTableView>
#include <QListView>

//KDE
#include <KDebug>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <KNotification>

//SFLPhone
#include "AccountWizard.h"
#include "SFLPhoneapplication.h"
#include "conf/ConfigurationDialog.h"
#include "conf/ConfigurationSkeleton.h"
#include "CallView.h"
#include "SFLPhone.h"
#include "AccountListModel.h"

//SFLPhone library
#include "lib/instance_interface_singleton.h"
#include "lib/sflphone_const.h"

static const char description[] = "A KDE 4 Client for SFLphone";

static const char version[] = "1.0.2";

int main(int argc, char **argv)
{
   
   try
   {
      KLocale::setMainCatalog("sflphone-client-kde");
      
      KAboutData about(
         "sflphone-client-kde"                      ,
         "sflphone-client-kde"                      ,
         ki18n("SFLphone KDE Client")               ,
         version                                    ,
         ki18n(description)                         ,
         KAboutData::License_GPL_V3                 ,
         ki18n("(C) 2009-2012 Savoir-faire Linux")  ,
         KLocalizedString()                         ,
         "http://www.sflphone.org."                 ,
         "sflphone@lists.savoirfairelinux.net"
      );
      about.addAuthor( ki18n( "Jérémy Quentin"         ), KLocalizedString(), "jeremy.quentin@savoirfairelinux.com"  );
      about.addAuthor( ki18n( "Emmanuel Lepage Vallee" ), KLocalizedString(), "emmanuel.lepage@savoirfairelinux.com" );
      //about.setTranslator( ki18nc("NAME OF TRANSLATORS","Your names"), ki18nc("EMAIL OF TRANSLATORS","Your emails") );
      KCmdLineArgs::init(argc, argv, &about);
      KCmdLineOptions options;
      KCmdLineArgs::addCmdLineOptions(options);
      
      //configuration dbus
      TreeWidgetCallModel::init();

      SFLPhoneApplication app;

      SFLPhone* sflphoneWindow_ = new SFLPhone();
      if( ! sflphoneWindow_->initialize() ) {
         exit(1);
         return 1;
      };
      sflphoneWindow_->show();
      
      int retVal = app.exec();
      
      ConfigurationSkeleton* conf = ConfigurationSkeleton::self();
      conf->writeConfig();
      delete sflphoneWindow_;
      return retVal;
   }
   catch(const char * msg)
   {
      kDebug() << msg;
   }
   catch(QString msg)
   {
      kDebug() << msg;
   }
} 
