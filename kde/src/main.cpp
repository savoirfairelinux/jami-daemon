#include <unistd.h>
#include <QApplication>
#include <QtCore/QString>
#include <QtGui/QMenu>
//#include <QtGui/QMenuBar>
#include <QtGui/QAction>

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <KNotification>

#include "AccountWizard.h"
#include "lib/instance_interface_singleton.h"
#include "lib/sflphone_const.h"
#include "SFLPhoneapplication.h"
#include "conf/ConfigurationDialog.h"
#include "conf/ConfigurationSkeleton.h"
#include "CallView.h"

#include <QTableView>
#include <QListView>
#include "AccountListModel.h"


static const char description[] = "A KDE 4 Client for SFLphone";

static const char version[] = "0.9.6";

int main(int argc, char **argv)
{
   
   try
   {
      KLocale::setMainCatalog("sflphone-client-kde");
      
      KAboutData about(
         "sflphone-client-kde", 
         "sflphone-client-kde", 
         ki18n("SFLphone KDE Client"), 
         version, 
         ki18n(description),
         KAboutData::License_GPL_V3, 
         ki18n("(C) 2009-2010 Savoir-faire Linux"), 
         KLocalizedString(), 
         "http://www.sflphone.org.", 
         "sflphone@lists.savoirfairelinux.net");
      about.addAuthor( ki18n("Jérémy Quentin"), KLocalizedString(), "jeremy.quentin@savoirfairelinux.com" );
      about.addAuthor( ki18n("Emmanuel Lepage Vallee"), KLocalizedString(), "emmanuel.lepage@savoirfairelinux.com" );
      about.setTranslator( ki18nc("NAME OF TRANSLATORS","Your names"), ki18nc("EMAIL OF TRANSLATORS","Your emails") );
      KCmdLineArgs::init(argc, argv, &about);
      KCmdLineOptions options;
      KCmdLineArgs::addCmdLineOptions(options);
      
      //configuration dbus
      CallView::init();

      SFLPhoneApplication app;

      
      int retVal = app.exec();
      
      ConfigurationSkeleton* conf = ConfigurationSkeleton::self();
      conf->writeConfig();
      return retVal;
   }
   catch(const char * msg)
   {
      qDebug() << msg;
   }
   catch(QString msg)
   {
      qDebug() << msg;
   }
} 
