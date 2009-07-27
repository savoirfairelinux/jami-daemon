#include <QApplication>
#include <QtCore/QString>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QAction>

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>

#include "SFLPhone.h"
#include "AccountWizard.h"
#include "instance_interface_singleton.h"
#include "sflphone_const.h"

#include "conf/ConfigurationDialog.h"

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
		   ki18n("(C) 2009 Savoir-faire Linux"), 
		   KLocalizedString(), 
		   "http://www.sflphone.org.", 
		   "sflphone@lists.savoirfairelinux.net");
		about.addAuthor( ki18n("Jérémy Quentin"), KLocalizedString(), "jeremy.quentin@savoirfairelinux.com" );
		about.setTranslator( ki18nc("NAME OF TRANSLATORS","Your names"), ki18nc("EMAIL OF TRANSLATORS","Your emails") );
		KCmdLineArgs::init(argc, argv, &about);
		KCmdLineOptions options;
		//options.add("+[URL]", ki18n( "Document to open" ));
		KCmdLineArgs::addCmdLineOptions(options);
		
		KApplication app;
		
		//configuration dbus
		registerCommTypes();
		new SFLPhone();

		InstanceInterface & instance = InstanceInterfaceSingleton::getInstance();
		instance.Register(getpid(), APP_NAME);
		
// 		SortableCodecListWidget * cl = new SortableCodecListWidget();
// 		cl->show();

// 		QListView * v = new QListView();
// 		v->setFlow(QListView::TopToBottom);
// 		v->setModel(new AccountListModel());
// 		v->show();
	
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
