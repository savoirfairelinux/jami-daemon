#include <QApplication>
#include <QtCore/QString>
#include <QtGui/QCursor>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QAction>

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>

#include "ConfigDialog.h"
#include "SFLPhone.h"
#include "AccountWizard.h"
#include "instance_interface_singleton.h"
#include "sflphone_const.h"

#include "conf/ConfigurationDialog.h"


static const char description[] = I18N_NOOP("A KDE 4 Client for SFLPhone");

static const char version[] = "0.9.5";

int main(int argc, char **argv)
{
	
	try
	{
		KLocale::setMainCatalog("sflphone-client-kde");
		qDebug() << KLocale::defaultLanguage();
		qDebug() << KLocale::defaultCountry();
		
		KAboutData about(
		   "sflphone-client-kde", 
		   0, 
		   ki18n("SFLPhone KDE Client"), 
		   version, 
		   ki18n(description),
		   KAboutData::License_GPL, 
		   ki18n("(C) 2009 Savoir-faire Linux"), 
		   KLocalizedString(), 
		   0, 
		   "jeremy.quentin@savoirfairelinux.com");
		
		about.addAuthor( ki18n("Jérémy Quentin"), KLocalizedString(), "jeremy.quentin@savoirfairelinux.com" );
		KCmdLineArgs::init(argc, argv, &about);
		KCmdLineOptions options;
		//options.add("+[URL]", ki18n( "Document to open" ));
		KCmdLineArgs::addCmdLineOptions(options);
		
		KApplication app;
		
		qDebug() << KGlobal::locale()->language();
		qDebug() << KGlobal::locale()->country();	
		//configuration dbus
		registerCommTypes();
		
		
		if(!QFile(QDir::homePath() + CONFIG_FILE_PATH).exists())
		{
			(new AccountWizard())->show();
		}
		
		InstanceInterface & instance = InstanceInterfaceSingleton::getInstance();
		instance.Register(getpid(), APP_NAME);
		
// 		ConfigurationDialogKDE * dlg = new ConfigurationDialogKDE();
// 		dlg->show();
		
		SFLPhone * fenetre = new SFLPhone();
		
		fenetre->move(QCursor::pos().x() - fenetre->geometry().width()/2, QCursor::pos().y() - fenetre->geometry().height()/2);
		fenetre->show();
	
		return app.exec();
	}
	catch(const char * msg)
	{
		printf("%s\n",msg);
	}
	catch(QString msg)
	{
		qDebug() << msg;
	}
} 
